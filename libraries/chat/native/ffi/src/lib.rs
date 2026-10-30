//! The C interface the viewer links (CHAT.md 4.5): five functions and a callback, JSON both ways, over
//! darkride-chat-core. include/darkride_chat.h declares them.

use std::{
    ffi::{CStr, CString, c_char, c_int, c_uint, c_void},
    sync::{Arc, Mutex},
    time::Duration,
};

use darkride_chat_core::{Chat, Config};

/// Called with each event as UTF-8 JSON, on one of the chat's own threads; the text is only valid during the call
pub type EventCallback = extern "C" fn(event: *const c_char, user_data: *mut c_void);

/// Where events go, until darkride_chat_free closes it: a thread of the chat's that finishes its work after the
/// runtime's shutdown stopped waiting for it (a big file still being written, a folder still being deleted) then
/// sends nothing, instead of calling into a host object that's gone
struct Target {
    to: Mutex<Option<(EventCallback, *mut c_void)>>,
}

// the host promises its callback and user data may be used from the chat's threads
unsafe impl Send for Target {}
unsafe impl Sync for Target {}

impl Target {
    fn send(&self, event: String) {
        let to = self.to.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        if let (Some((callback, user_data)), Ok(event)) = (*to, CString::new(event)) {
            callback(event.as_ptr(), user_data);
        }
    }

    fn close(&self) {
        *self.to.lock().unwrap_or_else(|poisoned| poisoned.into_inner()) = None;
    }
}

/// What the host holds as a DarkrideChat: the chat, and where its events go
pub struct Handle {
    chat: Chat,
    target: Arc<Target>,
}

/// The library's version, a static string
#[unsafe(no_mangle)]
pub extern "C" fn darkride_chat_version() -> *const c_char {
    concat!(env!("CARGO_PKG_VERSION"), "\0").as_ptr() as *const c_char
}

/// Starts the chat with a JSON config {homeserver, data_dir, device_name?}; null if the config is wrong. A session kept
/// in data_dir signs in by itself; otherwise a "state" event says signed_out.
///
/// # Safety
/// config is a NUL-terminated UTF-8 string; callback stays callable, and user_data valid, until darkride_chat_free.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn darkride_chat_new(config: *const c_char, callback: EventCallback, user_data: *mut c_void) -> *mut Handle {
    if config.is_null() {
        return std::ptr::null_mut();
    }
    let Ok(text) = (unsafe { CStr::from_ptr(config) }).to_str() else { return std::ptr::null_mut() };
    let Ok(config) = serde_json::from_str::<Config>(text) else { return std::ptr::null_mut() };
    let target = Arc::new(Target { to: Mutex::new(Some((callback, user_data))) });
    let sink = target.clone();
    let events = Arc::new(move |event: String| sink.send(event));
    match Chat::start(config, events) {
        Ok(chat) => Box::into_raw(Box::new(Handle { chat, target })),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Queues a command, JSON {"op": ...}; returns at once, the outcome comes as events
///
/// # Safety
/// chat is from darkride_chat_new and not freed; command is a NUL-terminated UTF-8 string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn darkride_chat_command(chat: *const Handle, command: *const c_char) {
    if chat.is_null() || command.is_null() {
        return;
    }
    let chat = unsafe { &(*chat).chat };
    match (unsafe { CStr::from_ptr(command) }).to_str() {
        Ok(text) => chat.command(text),
        Err(_) => chat.command("not UTF-8"),
    }
}

/// Waits up to timeout_ms until every command sent so far has finished: for a host about to stop, so a sign-out sent
/// at quit reaches the server. 1 if they all did, 0 if not
///
/// # Safety
/// chat is from darkride_chat_new and not freed; not called from inside the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn darkride_chat_wait_idle(chat: *const Handle, timeout_ms: c_uint) -> c_int {
    if chat.is_null() {
        return 1;
    }
    let chat = unsafe { &(*chat).chat };
    chat.wait_idle(Duration::from_millis(timeout_ms.into())) as c_int
}

/// Stops the chat (waiting up to two seconds for its threads); no callback comes after this returns
///
/// # Safety
/// chat is from darkride_chat_new, freed once, and not called from inside the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn darkride_chat_free(chat: *mut Handle) {
    if !chat.is_null() {
        let handle = unsafe { Box::from_raw(chat) };
        // closed first: nothing reaches the host from here on, even from a thread the shutdown leaves running
        handle.target.close();
        drop(handle);
    }
}
