/*
 *  hang.c
 *  tools/wasm-sdk/examples/c
 *
 *  A script that misbehaves on purpose: its update handler never returns. The host's time limit traps it,
 *  the log names the entry (on_signal) and the script stops; the client runs on. For the acceptance test of
 *  WASM-SCRIPTING.md round 2.
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#define DARKRIDE_IMPLEMENTATION
#include "../../c/darkride.h"

static volatile int keepGoing = 1;

void script_start(void) DARKRIDE_EXPORT("script_start");
void script_start(void) {
    dr_load_globals();
    dr_print("hang: subscribing to Script.update and looping forever in the handler");
    dr_connect_signal(dr_global("Script"), "update");
}

void on_signal(int32_t connection, const uint8_t* args, int32_t length) DARKRIDE_EXPORT("on_signal");
void on_signal(int32_t connection, const uint8_t* args, int32_t length) {
    (void)connection;
    (void)length;
    free((void*)args);  /* the arguments are the module's to free */
    while (keepGoing) {
    }
}

void on_callback(int32_t callback, const uint8_t* args, int32_t length) DARKRIDE_EXPORT("on_callback");
void on_callback(int32_t callback, const uint8_t* args, int32_t length) {
    (void)callback;
    (void)length;
    free((void*)args);  /* the arguments are the module's to free */
}

void script_stop(void) DARKRIDE_EXPORT("script_stop");
void script_stop(void) {
    dr_print("hang: script_stop");
}
