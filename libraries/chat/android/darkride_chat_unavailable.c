//
//  darkride_chat_unavailable.c
//  libraries/chat/android
//
//  The chat library's C interface without the library, for the Android app: matrix-rust-sdk isn't built for
//  Android yet (the app's NDK r22b is older than the r23 that current Rust's Android target links against), so
//  darkride_chat_new starts nothing and ChatService shows chat as unavailable.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "darkride_chat.h"

#include <stddef.h>

const char* darkride_chat_version(void) {
    return "unavailable";
}

DarkrideChat* darkride_chat_new(const char* config, DarkrideChatCallback callback, void* user_data) {
    (void)config;
    (void)callback;
    (void)user_data;
    return NULL;
}

void darkride_chat_command(const DarkrideChat* chat, const char* command) {
    (void)chat;
    (void)command;
}

int darkride_chat_wait_idle(const DarkrideChat* chat, unsigned int timeout_ms) {
    (void)chat;
    (void)timeout_ms;
    return 1;
}

void darkride_chat_free(DarkrideChat* chat) {
    (void)chat;
}
