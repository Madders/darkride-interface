/*
 *  assets.c
 *  tools/wasm-sdk/examples/c
 *
 *  A script in C that uses the Assets API, whose methods call a function of the script back: it compresses a
 *  text and decompresses it again (no server needed), checks that a bad request is refused with an error, and,
 *  when the domain has an asset server, uploads a text under a path with Assets.putAsset and reads it back with
 *  Assets.getAsset. Each step prints a line starting "assets:"; it begins three seconds after it starts and stops
 *  itself when it is done, or 20 seconds later. Built with build.sh (wasi-sdk) into assets.wasm.
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#define DARKRIDE_IMPLEMENTATION
#include "../../c/darkride.h"

#include <string.h>

/* the module's callback ids: the host calls on_callback with one of these */
enum { BEGIN = 1, COMPRESSED, DECOMPRESSED, PUT, GOT, GIVE_UP };

static const char* TEXT = "hello from a WebAssembly module, hello from a WebAssembly module";
static const char* ASSET_PATH = "/wasm-scripting/assets-test.txt";

static int32_t Script, Assets;

/* Assets.<method>(options, callback) where options is { data, responseType? } or { url, responseType } */
static bool request(const char* method, cbor_writer* options, int32_t callback) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_append(&args, options);
    cw_callback(&args, callback);
    dr_bytes result = dr_call_args(Assets, method, &args);
    cw_free(&args);
    char message[256];
    bool refused = dr_is_error(result, message, sizeof(message));
    if (refused) {
        dr_printf("assets: %s refused: %s", method, message);
    }
    dr_bytes_free(result);
    return !refused;
}

static void setTimer(int32_t callback, int milliseconds) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_callback(&args, callback);
    cw_int(&args, milliseconds);
    dr_bytes result = dr_call_args(Script, "setTimeout", &args);
    cw_free(&args);
    dr_bytes_free(result);
}

static void finish(const char* why) {
    dr_printf("assets: done (%s)", why);
    dr_bytes result = dr_call0(Script, "stop");
    dr_bytes_free(result);
}

void script_start(void) DARKRIDE_EXPORT("script_start");
void script_start(void) {
    dr_load_globals();
    Script = dr_global("Script");
    Assets = dr_global("Assets");
    if (!Script || !Assets) {
        dr_logf(DR_LOG_ERROR, "assets: missing globals (Script %d, Assets %d)", Script, Assets);
        return;
    }
    /* begin in three seconds, when the client has started (a client asked to quit while its default scripts are
       still starting can take long to stop) */
    setTimer(BEGIN, 3000);
}

static void begin(void) {
    /* a check that fails is an error the call raises, as it throws in a script */
    cbor_writer bad;
    cw_init(&bad);
    cw_array(&bad, 2);
    cw_text(&bad, "not a path");
    cw_callback(&bad, GOT);
    dr_bytes result = dr_call_args(Assets, "getMapping", &bad);
    cw_free(&bad);
    char message[256];
    dr_printf("assets: getMapping(\"not a path\") %s%s", dr_is_error(result, message, sizeof(message)) ? "is refused: " : "was not refused",
              dr_is_error(result, message, sizeof(message)) ? message : "");
    dr_bytes_free(result);

    /* compress a text; the outcome comes to on_callback(COMPRESSED) */
    cbor_writer options;
    cw_init(&options);
    cw_map(&options, 1);
    cw_key(&options, "data");
    cw_text(&options, TEXT);
    request("compressData", &options, COMPRESSED);
    cw_free(&options);

    /* the script stops itself if the asset server never answers */
    setTimer(GIVE_UP, 20000);
}

/* The outcome of an Assets call: [error or null, result]. Prints the error, and gives the result's member. */
static bool readOutcome(const uint8_t* payload, int32_t length, const char* what, const char* member, cbor_item* value) {
    cbor_reader r = cr_from_memory(payload, (size_t)length);
    cbor_item head, error;
    if (!cr_next(&r, &head) || head.type != CBOR_ARRAY || head.n < 2 || !cr_next(&r, &error)) {
        dr_printf("assets: %s: the callback's arguments are not [error, result]", what);
        return false;
    }
    if (error.type == CBOR_TEXT) {
        char text[256];
        cr_text(&error, text, sizeof(text));
        dr_printf("assets: %s failed: %s", what, text);
        return false;
    }
    if (error.type != CBOR_NULL) {
        dr_printf("assets: %s: the error is neither null nor text", what);
        return false;
    }
    if (!cr_find(&r, member, value)) {
        dr_printf("assets: %s: the result has no %s", what, member);
        return false;
    }
    return true;
}

void on_callback(int32_t callback, const uint8_t* payload, int32_t length) DARKRIDE_EXPORT("on_callback");
void on_callback(int32_t callback, const uint8_t* payload, int32_t length) {
    cbor_item value;
    cbor_writer options;
    switch (callback) {
        case BEGIN:
            begin();
            break;
        case COMPRESSED:
            if (readOutcome(payload, length, "compressData", "data", &value) && value.type == CBOR_BYTES) {
                dr_printf("assets: compressed %d bytes of text to %d bytes", (int)strlen(TEXT), (int)value.n);
                cw_init(&options);
                cw_map(&options, 2);
                cw_key(&options, "data");
                cw_bytes(&options, value.s, value.n);
                cw_key(&options, "responseType");
                cw_text(&options, "text");
                request("decompressData", &options, DECOMPRESSED);
                cw_free(&options);
            }
            break;
        case DECOMPRESSED:
            if (readOutcome(payload, length, "decompressData", "response", &value) && value.type == CBOR_TEXT) {
                bool same = value.n == strlen(TEXT) && memcmp(value.s, TEXT, value.n) == 0;
                dr_printf("assets: decompressed back to %d bytes, %s", (int)value.n, same ? "the same text" : "a different text");
            }
            /* now the asset server: upload the text under a path */
            cw_init(&options);
            cw_map(&options, 2);
            cw_key(&options, "data");
            cw_text(&options, TEXT);
            cw_key(&options, "path");
            cw_text(&options, ASSET_PATH);
            if (!request("putAsset", &options, PUT)) {
                finish("putAsset refused");
            }
            cw_free(&options);
            break;
        case PUT:
            if (readOutcome(payload, length, "putAsset", "hash", &value) && value.type == CBOR_TEXT) {
                char hash[80];
                cr_text(&value, hash, sizeof(hash));
                dr_printf("assets: uploaded as %s", hash);
                char url[256];
                snprintf(url, sizeof(url), "atp:%s", ASSET_PATH);
                cw_init(&options);
                cw_map(&options, 2);
                cw_key(&options, "url");
                cw_text(&options, url);
                cw_key(&options, "responseType");
                cw_text(&options, "text");
                request("getAsset", &options, GOT);
                cw_free(&options);
            } else {
                finish("no asset server");
            }
            break;
        case GOT:
            if (readOutcome(payload, length, "getAsset", "response", &value) && value.type == CBOR_TEXT) {
                bool same = value.n == strlen(TEXT) && memcmp(value.s, TEXT, value.n) == 0;
                dr_printf("assets: getAsset gave %d bytes, %s", (int)value.n, same ? "the text uploaded" : "a different text");
            }
            finish("getAsset answered");
            break;
        case GIVE_UP:
            finish("timed out");
            break;
        default:
            break;
    }
    free((void*)payload);
}

void script_stop(void) DARKRIDE_EXPORT("script_stop");
void script_stop(void) {
    dr_print("assets: bye");
    /* under --testScript the Test object exists: quit the client, as the test scripts do */
    int32_t Test = dr_global("Test");
    if (Test) {
        dr_bytes result = dr_call0(Test, "quit");
        dr_bytes_free(result);
    }
}
