/*
 *  darkride.h
 *  tools/wasm-sdk/c
 *
 *  The C side of the scripting host interface (ABI version 1), for a script module built with wasi-sdk as a
 *  reactor (-mexec-model=reactor). One header: the imports from module "darkride", the exports the host calls,
 *  a small CBOR writer and reader, and helpers for the common calls. Define DARKRIDE_IMPLEMENTATION in one
 *  source file before including it.
 *
 *  A script module exports script_start (called once the host has registered its objects), on_signal (a
 *  subscribed signal fired; the arguments are a CBOR array the host placed in the module's memory through
 *  alloc, and the module frees them), on_callback (a function the module gave the host is called; the same
 *  for its arguments), script_stop, and
 *  alloc and free for the host to place results in the module's memory. Every value crosses as CBOR; a
 *  returned i64 packs (pointer << 32) | length of CBOR bytes the module owns and frees.
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#ifndef DARKRIDE_H
#define DARKRIDE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DARKRIDE_ABI_VERSION 1

/* --- the imports ------------------------------------------------------------------------------------------- */

#define DARKRIDE_IMPORT(name) __attribute__((import_module("darkride"), import_name(name)))

int32_t dr_version(void) DARKRIDE_IMPORT("version");
int64_t dr_globals(void) DARKRIDE_IMPORT("globals");
int64_t dr_describe(const void* cbor, int32_t length) DARKRIDE_IMPORT("describe");
int64_t dr_call(int32_t object, const char* method, int32_t method_length, const void* args, int32_t args_length) DARKRIDE_IMPORT("call");
int64_t dr_get(int32_t object, const char* property, int32_t property_length) DARKRIDE_IMPORT("get");
int64_t dr_set(int32_t object, const char* property, int32_t property_length, const void* value, int32_t value_length) DARKRIDE_IMPORT("set");
int32_t dr_connect(int32_t object, const char* signal, int32_t signal_length) DARKRIDE_IMPORT("connect");
void dr_disconnect(int32_t connection) DARKRIDE_IMPORT("disconnect");
void dr_release(int32_t object) DARKRIDE_IMPORT("release");
void dr_log(int32_t level, const char* text, int32_t length) DARKRIDE_IMPORT("log");
/* 1 when the running call should stop now (the script is stopping, or the call ran past its time limit). Only a
   module the host runs with cooperative interruption needs to ask (the JavaScript guest does, from QuickJS's
   interrupt handler); every other module is stopped by the runtime itself. */
int32_t dr_interrupted(void) DARKRIDE_IMPORT("interrupted");

enum { DR_LOG_PRINT = 0, DR_LOG_INFO = 1, DR_LOG_WARNING = 2, DR_LOG_ERROR = 3, DR_LOG_DEBUG = 4 };

/* --- the exports (defined by the module; the attribute names them for the host) ----------------------------- */

#define DARKRIDE_EXPORT(name) __attribute__((export_name(name)))

/* --- bytes the host returned ----------------------------------------------------------------------------------- */
/* A packed result is a pointer and a length in the module's memory, the module's to free. An empty result (0) is
   nothing: what a method that returns nothing gives, and a property write that worked. */

typedef struct {
    uint8_t* data;
    int32_t length;
} dr_bytes;

static inline dr_bytes dr_unpack(int64_t packed) {
    dr_bytes bytes;
    bytes.data = (uint8_t*)(uintptr_t)(uint32_t)((uint64_t)packed >> 32);
    bytes.length = (int32_t)(uint32_t)((uint64_t)packed & 0xffffffffu);
    if (packed == 0) {
        bytes.data = NULL;
        bytes.length = 0;
    }
    return bytes;
}

static inline void dr_bytes_free(dr_bytes bytes) {
    free(bytes.data);
}

/* --- CBOR writer ------------------------------------------------------------------------------------------------ */

typedef struct {
    uint8_t* data;
    size_t length;
    size_t capacity;
} cbor_writer;

void cw_init(cbor_writer* w);
void cw_free(cbor_writer* w);
void cw_int(cbor_writer* w, int64_t value);
void cw_double(cbor_writer* w, double value);
void cw_bool(cbor_writer* w, bool value);
void cw_null(cbor_writer* w);
void cw_text(cbor_writer* w, const char* text);
void cw_text_n(cbor_writer* w, const char* text, size_t length);
void cw_bytes(cbor_writer* w, const void* data, size_t length);
void cw_array(cbor_writer* w, size_t count);  /* followed by `count` items */
void cw_map(cbor_writer* w, size_t count);    /* followed by `count` key, value pairs */
void cw_key(cbor_writer* w, const char* key); /* a text key */
/* an object handle as the host expects it: {"@object": handle} */
void cw_object(cbor_writer* w, int32_t handle);
/* a function of this module, by the id the module chooses: {"@callback": id} */
void cw_callback(cbor_writer* w, int32_t id);
/* the items another writer holds, as they are (an options map written apart, then put into the arguments) */
void cw_append(cbor_writer* w, const cbor_writer* items);

typedef struct { double x, y, z; } dr_vec3;
typedef struct { double x, y, z, w; } dr_quat;
void cw_vec3(cbor_writer* w, dr_vec3 v);
void cw_quat(cbor_writer* w, dr_quat q);

/* --- CBOR reader ------------------------------------------------------------------------------------------------ */

typedef enum {
    CBOR_UINT, CBOR_NINT, CBOR_BYTES, CBOR_TEXT, CBOR_ARRAY, CBOR_MAP, CBOR_TAG,
    CBOR_FALSE, CBOR_TRUE, CBOR_NULL, CBOR_UNDEFINED, CBOR_FLOAT, CBOR_END, CBOR_ERROR
} cbor_type;

typedef struct {
    cbor_type type;
    int64_t integer;    /* UINT, NINT (the value, negative), TAG (the tag) */
    double number;      /* FLOAT, and the integer as a double for UINT and NINT */
    const uint8_t* s;   /* BYTES, TEXT: the content (not terminated) */
    size_t n;           /* BYTES, TEXT: the length; ARRAY, MAP: the count */
} cbor_item;

typedef struct {
    const uint8_t* p;
    const uint8_t* end;
} cbor_reader;

static inline cbor_reader cr_from(dr_bytes bytes) {
    cbor_reader r = { bytes.data, bytes.data ? bytes.data + bytes.length : NULL };
    return r;
}
static inline cbor_reader cr_from_memory(const void* data, size_t length) {
    cbor_reader r = { (const uint8_t*)data, (const uint8_t*)data + length };
    return r;
}
/* Reads the next item's head: for an array or map, the items follow and `n` says how many. */
bool cr_next(cbor_reader* r, cbor_item* item);
/* Skips a whole item, however nested. */
bool cr_skip(cbor_reader* r);
/* Positions the reader on the value of `key` in the map at the reader's position (the reader is left after
   the head of that value's item, so cr_next reads the value); false when the key is absent. The reader must
   be on a map head. */
bool cr_find(cbor_reader* r, const char* key, cbor_item* value);
/* A number (an integer or a float) or 0. */
static inline double cr_number(const cbor_item* item) {
    return (item->type == CBOR_UINT || item->type == CBOR_NINT || item->type == CBOR_FLOAT) ? item->number : 0.0;
}
/* A text item copied into a buffer, terminated; the number of bytes copied. */
size_t cr_text(const cbor_item* item, char* out, size_t capacity);
/* A {x, y, z} map read into a vec3; false when the item is not such a map. */
bool cr_vec3(cbor_reader* r, dr_vec3* out);

/* --- helpers ---------------------------------------------------------------------------------------------------- */

/* Loads the globals' handles; then dr_global("MyAvatar") answers. */
void dr_load_globals(void);
int32_t dr_global(const char* name);

/* A call with the arguments written into `args` (an array the caller started with cw_array); the result bytes,
   to free with dr_bytes_free. */
dr_bytes dr_call_args(int32_t object, const char* method, const cbor_writer* args);
/* Calls with no arguments. */
dr_bytes dr_call0(int32_t object, const char* method);
dr_bytes dr_get_property(int32_t object, const char* property);
bool dr_set_property(int32_t object, const char* property, const cbor_writer* value);
int32_t dr_connect_signal(int32_t object, const char* signal);
/* True when the result is {"@error": ...}; the message copied out when a buffer is given. */
bool dr_is_error(dr_bytes result, char* message, size_t capacity);
/* The result as a number, text, vec3, or an object handle; 0 / false when it is not one. */
double dr_result_number(dr_bytes result);
size_t dr_result_text(dr_bytes result, char* out, size_t capacity);
bool dr_result_vec3(dr_bytes result, dr_vec3* out);
int32_t dr_result_object(dr_bytes result);
/* True when the result is null or empty: what the host gives for a value it has none of, such as the id of an entity
   it could not add (Entities.addEntity before the world has loaded). */
bool dr_result_is_null(dr_bytes result);

void dr_print(const char* text);
void dr_printf(const char* format, ...);
void dr_logf(int32_t level, const char* format, ...);

/* --- the entity-script contract ------------------------------------------------------------------------------------
   A module that runs as an entity script (an entity's `script` or `serverScripts` property naming a .wasm URL)
   exports, beside the usual ones: entity_load(ptr, len) -> i64, entity_unload(ptr, len), entity_has(ptr, len)
   -> i32 and entity_call(ptr, len) -> i64. Each takes one CBOR map the host placed in the module's memory
   (the module's to free): {"entity": id, "url": url} for load, {"entity": id} for unload, {"entity": id,
   "method": name} for has, and {"entity": id, "method": name, "args": [...], "remoteCallerID"?: id} for call.
   entity_load returns null or an error record, entity_call the method's result or 0, both packed as a host
   function's result is (the host frees them). One instance of the module serves every entity that runs it;
   the entity's id (text, "{...}") is the key of whatever state the module keeps per entity. The host calls
   preload after load and unload before unload through entity_call, as it calls every method. */

typedef struct {
    char entity[64];      /* the entity's id */
    char url[1024];       /* entity_load: the module's URL */
    char method[128];     /* entity_has, entity_call */
    char remoteCaller[64];/* entity_call: the caller's id, empty when local */
    cbor_reader args;     /* entity_call: positioned on the "args" array's head (cr_next reads it); p NULL when absent */
} dr_entity_payload;

/* Reads a contract payload; the payload bytes stay the caller's to free after the reader is done. */
bool dr_entity_payload_read(const uint8_t* payload, int32_t length, dr_entity_payload* out);
/* A value packed for the host as a result: the bytes of a writer copied into memory the host frees. */
int64_t dr_pack_result(const cbor_writer* w);
/* An error record packed for the host: {"@error": {"type", "message"}}. */
int64_t dr_pack_error(const char* type, const char* message);

/* --- implementation --------------------------------------------------------------------------------------------- */

#ifdef DARKRIDE_IMPLEMENTATION

void* dr_alloc(int32_t size) DARKRIDE_EXPORT("alloc");
void* dr_alloc(int32_t size) {
    return malloc(size > 0 ? (size_t)size : 1);
}

void dr_free(void* pointer, int32_t size) DARKRIDE_EXPORT("free");
void dr_free(void* pointer, int32_t size) {
    (void)size;
    free(pointer);
}

static void cw_reserve(cbor_writer* w, size_t more) {
    if (w->length + more <= w->capacity) {
        return;
    }
    size_t capacity = w->capacity ? w->capacity * 2 : 64;
    while (capacity < w->length + more) {
        capacity *= 2;
    }
    w->data = (uint8_t*)realloc(w->data, capacity);
    w->capacity = capacity;
}

static void cw_byte(cbor_writer* w, uint8_t byte) {
    cw_reserve(w, 1);
    w->data[w->length++] = byte;
}

static void cw_head(cbor_writer* w, uint8_t major, uint64_t value) {
    major = (uint8_t)(major << 5);
    if (value < 24) {
        cw_byte(w, (uint8_t)(major | value));
    } else if (value <= 0xff) {
        cw_byte(w, major | 24);
        cw_byte(w, (uint8_t)value);
    } else if (value <= 0xffff) {
        cw_byte(w, major | 25);
        cw_byte(w, (uint8_t)(value >> 8));
        cw_byte(w, (uint8_t)value);
    } else if (value <= 0xffffffffu) {
        cw_byte(w, major | 26);
        for (int shift = 24; shift >= 0; shift -= 8) {
            cw_byte(w, (uint8_t)(value >> shift));
        }
    } else {
        cw_byte(w, major | 27);
        for (int shift = 56; shift >= 0; shift -= 8) {
            cw_byte(w, (uint8_t)(value >> shift));
        }
    }
}

void cw_init(cbor_writer* w) {
    w->data = NULL;
    w->length = 0;
    w->capacity = 0;
}

void cw_free(cbor_writer* w) {
    free(w->data);
    cw_init(w);
}

void cw_int(cbor_writer* w, int64_t value) {
    if (value >= 0) {
        cw_head(w, 0, (uint64_t)value);
    } else {
        cw_head(w, 1, (uint64_t)(-1 - value));
    }
}

void cw_double(cbor_writer* w, double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    cw_byte(w, 0xfb);
    for (int shift = 56; shift >= 0; shift -= 8) {
        cw_byte(w, (uint8_t)(bits >> shift));
    }
}

void cw_bool(cbor_writer* w, bool value) {
    cw_byte(w, value ? 0xf5 : 0xf4);
}

void cw_null(cbor_writer* w) {
    cw_byte(w, 0xf6);
}

void cw_text_n(cbor_writer* w, const char* text, size_t length) {
    cw_head(w, 3, length);
    cw_reserve(w, length);
    memcpy(w->data + w->length, text, length);
    w->length += length;
}

void cw_text(cbor_writer* w, const char* text) {
    cw_text_n(w, text, strlen(text));
}

void cw_bytes(cbor_writer* w, const void* data, size_t length) {
    cw_head(w, 2, length);
    cw_reserve(w, length);
    memcpy(w->data + w->length, data, length);
    w->length += length;
}

void cw_array(cbor_writer* w, size_t count) {
    cw_head(w, 4, count);
}

void cw_map(cbor_writer* w, size_t count) {
    cw_head(w, 5, count);
}

void cw_key(cbor_writer* w, const char* key) {
    cw_text(w, key);
}

void cw_object(cbor_writer* w, int32_t handle) {
    cw_map(w, 1);
    cw_key(w, "@object");
    cw_int(w, handle);
}

void cw_callback(cbor_writer* w, int32_t id) {
    cw_map(w, 1);
    cw_key(w, "@callback");
    cw_int(w, id);
}

void cw_append(cbor_writer* w, const cbor_writer* items) {
    cw_reserve(w, items->length);
    memcpy(w->data + w->length, items->data, items->length);
    w->length += items->length;
}

void cw_vec3(cbor_writer* w, dr_vec3 v) {
    cw_map(w, 3);
    cw_key(w, "x");
    cw_double(w, v.x);
    cw_key(w, "y");
    cw_double(w, v.y);
    cw_key(w, "z");
    cw_double(w, v.z);
}

void cw_quat(cbor_writer* w, dr_quat q) {
    cw_map(w, 4);
    cw_key(w, "x");
    cw_double(w, q.x);
    cw_key(w, "y");
    cw_double(w, q.y);
    cw_key(w, "z");
    cw_double(w, q.z);
    cw_key(w, "w");
    cw_double(w, q.w);
}

static bool cr_read_uint(cbor_reader* r, uint8_t info, uint64_t* value) {
    if (info < 24) {
        *value = info;
        return true;
    }
    size_t length = info == 24 ? 1 : info == 25 ? 2 : info == 26 ? 4 : info == 27 ? 8 : 0;
    if (length == 0 || r->p + length > r->end) {
        return false;
    }
    uint64_t v = 0;
    for (size_t i = 0; i < length; ++i) {
        v = (v << 8) | r->p[i];
    }
    r->p += length;
    *value = v;
    return true;
}

bool cr_next(cbor_reader* r, cbor_item* item) {
    memset(item, 0, sizeof(*item));
    if (!r->p || r->p >= r->end) {
        item->type = CBOR_END;
        return false;
    }
    uint8_t head = *r->p++;
    uint8_t major = head >> 5;
    uint8_t info = head & 0x1f;
    uint64_t value = 0;
    switch (major) {
        case 0:
            if (!cr_read_uint(r, info, &value)) break;
            item->type = CBOR_UINT;
            item->integer = (int64_t)value;
            item->number = (double)value;
            return true;
        case 1:
            if (!cr_read_uint(r, info, &value)) break;
            item->type = CBOR_NINT;
            item->integer = -1 - (int64_t)value;
            item->number = (double)item->integer;
            return true;
        case 2:
        case 3:
            if (!cr_read_uint(r, info, &value) || r->p + value > r->end) break;
            item->type = major == 2 ? CBOR_BYTES : CBOR_TEXT;
            item->s = r->p;
            item->n = (size_t)value;
            r->p += value;
            return true;
        case 4:
        case 5:
            if (!cr_read_uint(r, info, &value)) break;
            item->type = major == 4 ? CBOR_ARRAY : CBOR_MAP;
            item->n = (size_t)value;
            return true;
        case 6:
            if (!cr_read_uint(r, info, &value)) break;
            item->type = CBOR_TAG;
            item->integer = (int64_t)value;
            return true;
        case 7:
            if (info == 20) { item->type = CBOR_FALSE; return true; }
            if (info == 21) { item->type = CBOR_TRUE; return true; }
            if (info == 22) { item->type = CBOR_NULL; return true; }
            if (info == 23) { item->type = CBOR_UNDEFINED; return true; }
            if (info == 25) {
                if (r->p + 2 > r->end) break;
                uint16_t h = (uint16_t)((r->p[0] << 8) | r->p[1]);
                r->p += 2;
                int exponent = (h >> 10) & 0x1f;
                int mantissa = h & 0x3ff;
                double v;
                if (exponent == 0) v = mantissa * 5.9604644775390625e-8;          /* subnormal: 2^-24 */
                else if (exponent != 31) v = (mantissa + 1024.0) * 0.0009765625;  /* 2^-10 */
                else v = mantissa ? __builtin_nan("") : __builtin_inf();
                if (exponent != 0 && exponent != 31) {
                    int shift = exponent - 15;
                    while (shift > 0) { v *= 2.0; --shift; }
                    while (shift < 0) { v *= 0.5; ++shift; }
                }
                item->type = CBOR_FLOAT;
                item->number = (h & 0x8000) ? -v : v;
                return true;
            }
            if (info == 26) {
                if (r->p + 4 > r->end) break;
                uint32_t bits = 0;
                for (int i = 0; i < 4; ++i) bits = (bits << 8) | r->p[i];
                r->p += 4;
                float f;
                memcpy(&f, &bits, sizeof(f));
                item->type = CBOR_FLOAT;
                item->number = f;
                return true;
            }
            if (info == 27) {
                if (r->p + 8 > r->end) break;
                uint64_t bits = 0;
                for (int i = 0; i < 8; ++i) bits = (bits << 8) | r->p[i];
                r->p += 8;
                double d;
                memcpy(&d, &bits, sizeof(d));
                item->type = CBOR_FLOAT;
                item->number = d;
                return true;
            }
            break;
        default:
            break;
    }
    item->type = CBOR_ERROR;
    r->p = r->end;
    return false;
}

bool cr_skip(cbor_reader* r) {
    cbor_item item;
    if (!cr_next(r, &item)) {
        return false;
    }
    size_t pending = 0;
    if (item.type == CBOR_ARRAY) pending = item.n;
    else if (item.type == CBOR_MAP) pending = item.n * 2;
    else if (item.type == CBOR_TAG) pending = 1;
    for (size_t i = 0; i < pending; ++i) {
        if (!cr_skip(r)) {
            return false;
        }
    }
    return true;
}

bool cr_find(cbor_reader* r, const char* key, cbor_item* value) {
    cbor_item head;
    if (!cr_next(r, &head) || head.type != CBOR_MAP) {
        return false;
    }
    size_t length = strlen(key);
    for (size_t i = 0; i < head.n; ++i) {
        cbor_item k;
        if (!cr_next(r, &k)) {
            return false;
        }
        if (k.type == CBOR_TEXT && k.n == length && memcmp(k.s, key, length) == 0) {
            return cr_next(r, value);
        }
        if (!cr_skip(r)) {
            return false;
        }
    }
    return false;
}

size_t cr_text(const cbor_item* item, char* out, size_t capacity) {
    if (item->type != CBOR_TEXT || capacity == 0) {
        if (capacity) out[0] = 0;
        return 0;
    }
    size_t n = item->n < capacity - 1 ? item->n : capacity - 1;
    memcpy(out, item->s, n);
    out[n] = 0;
    return n;
}

bool cr_vec3(cbor_reader* r, dr_vec3* out) {
    cbor_item head;
    if (!cr_next(r, &head) || head.type != CBOR_MAP) {
        return false;
    }
    out->x = out->y = out->z = 0.0;
    for (size_t i = 0; i < head.n; ++i) {
        cbor_item k, v;
        if (!cr_next(r, &k) || !cr_next(r, &v)) {
            return false;
        }
        if (k.type == CBOR_TEXT && k.n == 1) {
            if (k.s[0] == 'x') out->x = cr_number(&v);
            else if (k.s[0] == 'y') out->y = cr_number(&v);
            else if (k.s[0] == 'z') out->z = cr_number(&v);
        }
    }
    return true;
}

bool dr_entity_payload_read(const uint8_t* payload, int32_t length, dr_entity_payload* out) {
    memset(out, 0, sizeof(*out));
    cbor_reader r = cr_from_memory(payload, (size_t)length);
    cbor_item head;
    if (!cr_next(&r, &head) || head.type != CBOR_MAP) {
        return false;
    }
    for (size_t i = 0; i < head.n; ++i) {
        cbor_item key;
        if (!cr_next(&r, &key) || key.type != CBOR_TEXT) {
            return false;
        }
        if (key.n == 6 && memcmp(key.s, "entity", 6) == 0) {
            cbor_item v; if (!cr_next(&r, &v)) return false; cr_text(&v, out->entity, sizeof(out->entity));
        } else if (key.n == 3 && memcmp(key.s, "url", 3) == 0) {
            cbor_item v; if (!cr_next(&r, &v)) return false; cr_text(&v, out->url, sizeof(out->url));
        } else if (key.n == 6 && memcmp(key.s, "method", 6) == 0) {
            cbor_item v; if (!cr_next(&r, &v)) return false; cr_text(&v, out->method, sizeof(out->method));
        } else if (key.n == 14 && memcmp(key.s, "remoteCallerID", 14) == 0) {
            cbor_item v; if (!cr_next(&r, &v)) return false; cr_text(&v, out->remoteCaller, sizeof(out->remoteCaller));
        } else if (key.n == 4 && memcmp(key.s, "args", 4) == 0) {
            out->args = r;  /* positioned on the array's head */
            if (!cr_skip(&r)) return false;
        } else {
            if (!cr_skip(&r)) return false;
        }
    }
    return out->entity[0] != 0;
}

int64_t dr_pack_result(const cbor_writer* w) {
    if (!w || w->length == 0) {
        return 0;
    }
    uint8_t* out = (uint8_t*)malloc(w->length);
    if (!out) {
        return 0;
    }
    memcpy(out, w->data, w->length);
    return ((int64_t)(uint32_t)(uintptr_t)out << 32) | (int64_t)(uint32_t)w->length;
}

int64_t dr_pack_error(const char* type, const char* message) {
    cbor_writer w;
    cw_init(&w);
    cw_map(&w, 1);
    cw_key(&w, "@error");
    cw_map(&w, 2);
    cw_key(&w, "type");
    cw_text(&w, type ? type : "Error");
    cw_key(&w, "message");
    cw_text(&w, message ? message : "");
    int64_t packed = dr_pack_result(&w);
    cw_free(&w);
    return packed;
}

#define DR_MAX_GLOBALS 256
static struct { char name[48]; int32_t handle; } dr_global_table[DR_MAX_GLOBALS];
static int dr_global_count = 0;

void dr_load_globals(void) {
    dr_bytes bytes = dr_unpack(dr_globals());
    cbor_reader r = cr_from(bytes);
    cbor_item head;
    dr_global_count = 0;
    if (cr_next(&r, &head) && head.type == CBOR_MAP) {
        for (size_t i = 0; i < head.n && dr_global_count < DR_MAX_GLOBALS; ++i) {
            cbor_item k, v;
            if (!cr_next(&r, &k) || !cr_next(&r, &v)) break;
            cr_text(&k, dr_global_table[dr_global_count].name, sizeof(dr_global_table[0].name));
            dr_global_table[dr_global_count].handle = (int32_t)v.integer;
            ++dr_global_count;
        }
    }
    dr_bytes_free(bytes);
}

int32_t dr_global(const char* name) {
    for (int i = 0; i < dr_global_count; ++i) {
        if (strcmp(dr_global_table[i].name, name) == 0) {
            return dr_global_table[i].handle;
        }
    }
    return 0;
}

dr_bytes dr_call_args(int32_t object, const char* method, const cbor_writer* args) {
    return dr_unpack(dr_call(object, method, (int32_t)strlen(method), args->data, (int32_t)args->length));
}

dr_bytes dr_call0(int32_t object, const char* method) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 0);
    dr_bytes result = dr_call_args(object, method, &args);
    cw_free(&args);
    return result;
}

dr_bytes dr_get_property(int32_t object, const char* property) {
    return dr_unpack(dr_get(object, property, (int32_t)strlen(property)));
}

bool dr_set_property(int32_t object, const char* property, const cbor_writer* value) {
    dr_bytes result = dr_unpack(dr_set(object, property, (int32_t)strlen(property), value->data, (int32_t)value->length));
    bool failed = dr_is_error(result, NULL, 0);
    dr_bytes_free(result);
    return !failed;
}

int32_t dr_connect_signal(int32_t object, const char* signal) {
    return dr_connect(object, signal, (int32_t)strlen(signal));
}

bool dr_is_error(dr_bytes result, char* message, size_t capacity) {
    if (capacity) message[0] = 0;
    cbor_reader r = cr_from(result);
    cbor_item error;
    if (!cr_find(&r, "@error", &error) || error.type != CBOR_MAP) {
        return false;
    }
    if (capacity) {
        /* the reader is past the head of the error map: scan its pairs for "message" */
        for (size_t i = 0; i < error.n; ++i) {
            cbor_item k, v;
            if (!cr_next(&r, &k) || !cr_next(&r, &v)) break;
            if (k.type == CBOR_TEXT && k.n == 7 && memcmp(k.s, "message", 7) == 0) {
                cr_text(&v, message, capacity);
            }
        }
    }
    return true;
}

double dr_result_number(dr_bytes result) {
    cbor_reader r = cr_from(result);
    cbor_item item;
    return cr_next(&r, &item) ? cr_number(&item) : 0.0;
}

size_t dr_result_text(dr_bytes result, char* out, size_t capacity) {
    cbor_reader r = cr_from(result);
    cbor_item item;
    if (!cr_next(&r, &item)) {
        if (capacity) out[0] = 0;
        return 0;
    }
    return cr_text(&item, out, capacity);
}

bool dr_result_vec3(dr_bytes result, dr_vec3* out) {
    cbor_reader r = cr_from(result);
    return cr_vec3(&r, out);
}

int32_t dr_result_object(dr_bytes result) {
    cbor_reader r = cr_from(result);
    cbor_item handle;
    if (!cr_find(&r, "@object", &handle)) {
        return 0;
    }
    return (int32_t)handle.integer;
}

bool dr_result_is_null(dr_bytes result) {
    cbor_reader r = cr_from(result);
    cbor_item item;
    return !cr_next(&r, &item) || item.type == CBOR_NULL || item.type == CBOR_UNDEFINED;
}

void dr_print(const char* text) {
    dr_log(DR_LOG_PRINT, text, (int32_t)strlen(text));
}

void dr_logf(int32_t level, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n < 0) return;
    dr_log(level, buffer, n < (int)sizeof(buffer) ? n : (int32_t)sizeof(buffer) - 1);
}

void dr_printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n < 0) return;
    dr_log(DR_LOG_PRINT, buffer, n < (int)sizeof(buffer) ? n : (int32_t)sizeof(buffer) - 1);
}

#endif /* DARKRIDE_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* DARKRIDE_H */
