/*
 *  alert.c
 *  tools/wasm-sdk/examples/c
 *
 *  A call that blocks on the main thread from a module: Window.alert shows a dialog and returns when it is
 *  closed; the script then stops. For the acceptance test of WASM-SCRIPTING.md round 2 (a person closes the
 *  dialog, so this one is run by hand, not by the harness).
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#define DARKRIDE_IMPLEMENTATION
#include "../../c/darkride.h"

void script_start(void) DARKRIDE_EXPORT("script_start");
void script_start(void) {
    dr_load_globals();
    int32_t Window = dr_global("Window");
    int32_t Script = dr_global("Script");
    dr_print("alert: calling Window.alert (it returns when the dialog is closed)");
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 1);
    cw_text(&args, "Hello from a WebAssembly script. Close me and the script stops.");
    dr_bytes result = dr_call_args(Window, "alert", &args);
    cw_free(&args);
    char message[256];
    if (dr_is_error(result, message, sizeof(message))) {
        dr_logf(DR_LOG_ERROR, "alert: Window.alert failed: %s", message);
    } else {
        dr_print("alert: the dialog was closed");
    }
    dr_bytes_free(result);
    result = dr_call0(Script, "stop");
    dr_bytes_free(result);
}

void on_signal(int32_t connection, const uint8_t* args, int32_t length) DARKRIDE_EXPORT("on_signal");
void on_signal(int32_t connection, const uint8_t* args, int32_t length) {
    (void)connection;
    (void)length;
    free((void*)args);  /* the arguments are the module's to free */
}

void on_callback(int32_t callback, const uint8_t* args, int32_t length) DARKRIDE_EXPORT("on_callback");
void on_callback(int32_t callback, const uint8_t* args, int32_t length) {
    (void)callback;
    (void)length;
    free((void*)args);  /* the arguments are the module's to free */
}
