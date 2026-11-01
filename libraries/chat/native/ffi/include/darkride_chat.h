/* The viewer's chat (darkride_chat/native, CHAT.md 4.5): Matrix through matrix-rust-sdk. Commands go in as JSON,
 * events come out as JSON through the callback, on the chat's own threads; core/src/lib.rs lists both. */
#ifndef DARKRIDE_CHAT_H
#define DARKRIDE_CHAT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DarkrideChat DarkrideChat;

/* each event as UTF-8 JSON, valid only during the call */
typedef void (*DarkrideChatCallback)(const char* event, void* user_data);

const char* darkride_chat_version(void);

/* config: {"homeserver": "https://chat.darkride.ai", "data_dir": "...", "device_name": "..."}; NULL if it is wrong */
DarkrideChat* darkride_chat_new(const char* config, DarkrideChatCallback callback, void* user_data);

/* {"op": "login_jwt", "token": "..."}, {"op": "open", "room": "#lobby:darkride.ai"}, ...; returns at once */
void darkride_chat_command(const DarkrideChat* chat, const char* command);

/* waits up to timeout_ms until every command sent so far has finished (a sign-out sent at quit reaches the server);
 * 1 if they all did, 0 if not; not from inside the callback */
int darkride_chat_wait_idle(const DarkrideChat* chat, unsigned int timeout_ms);

/* stops it; no callback comes after this returns; not from inside the callback */
void darkride_chat_free(DarkrideChat* chat);

#ifdef __cplusplus
}
#endif

#endif
