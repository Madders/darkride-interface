/*
 *  entity.c
 *  tools/wasm-sdk/examples/c
 *
 *  An entity script in C: set an entity's `script` (on a client) or `serverScripts` (on the entity script
 *  server) property to this module's URL (entity.wasm) and the engine loads one instance of it for every
 *  such entity. It counts the entities it serves, greets each in preload, answers clicks by changing the
 *  entity's colour, reports collisions, and offers one method to remote callers (`ping`, which writes who
 *  called into the entity's name). Built with build.sh (wasi-sdk) into entity.wasm.
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#define DARKRIDE_IMPLEMENTATION
#include "../../c/darkride.h"

#define MAX_ENTITIES 64

static int32_t Entities;
static struct {
    char id[64];
    int clicks;
    int collisions;
} entities[MAX_ENTITIES];
static int entityCount;

static int indexOf(const char* id) {
    for (int i = 0; i < entityCount; ++i) {
        if (strcmp(entities[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

/* Entities.editEntity(id, {name: text}) and {color: {...}}: the entity shows what happened. */
static void setName(const char* id, const char* name) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_text(&args, id);
    cw_map(&args, 1);
    cw_key(&args, "name");
    cw_text(&args, name);
    dr_bytes result = dr_call_args(Entities, "editEntity", &args);
    cw_free(&args);
    dr_bytes_free(result);
}

static void setColor(const char* id, int red, int green, int blue) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_text(&args, id);
    cw_map(&args, 1);
    cw_key(&args, "color");
    cw_map(&args, 3);
    cw_key(&args, "red");
    cw_int(&args, red);
    cw_key(&args, "green");
    cw_int(&args, green);
    cw_key(&args, "blue");
    cw_int(&args, blue);
    dr_bytes result = dr_call_args(Entities, "editEntity", &args);
    cw_free(&args);
    dr_bytes_free(result);
}

void script_start(void) DARKRIDE_EXPORT("script_start");
void script_start(void) {
    dr_load_globals();
    Entities = dr_global("Entities");
    dr_printf("entity.wasm: module started (%s)", Entities ? "Entities found" : "no Entities object");
}

void script_stop(void) DARKRIDE_EXPORT("script_stop");
void script_stop(void) {
    dr_printf("entity.wasm: module stopped after serving %d entities", entityCount);
}

/* entity_load: one entity more; its state is kept here by id. */
int64_t entity_load(uint8_t* payload, int32_t length) DARKRIDE_EXPORT("entity_load");
int64_t entity_load(uint8_t* payload, int32_t length) {
    dr_entity_payload p;
    bool ok = dr_entity_payload_read(payload, length, &p);
    free(payload);
    if (!ok) {
        return dr_pack_error("Error", "entity.wasm: bad load payload");
    }
    if (indexOf(p.entity) < 0) {
        if (entityCount >= MAX_ENTITIES) {
            return dr_pack_error("RangeError", "entity.wasm: too many entities for this module");
        }
        strncpy(entities[entityCount].id, p.entity, sizeof(entities[entityCount].id) - 1);
        entities[entityCount].clicks = 0;
        entities[entityCount].collisions = 0;
        ++entityCount;
    }
    dr_printf("entity.wasm: loaded for %s (%d entities)", p.entity, entityCount);
    return 0;
}

void entity_unload(uint8_t* payload, int32_t length) DARKRIDE_EXPORT("entity_unload");
void entity_unload(uint8_t* payload, int32_t length) {
    dr_entity_payload p;
    bool ok = dr_entity_payload_read(payload, length, &p);
    free(payload);
    if (!ok) {
        return;
    }
    int i = indexOf(p.entity);
    if (i >= 0) {
        entities[i] = entities[entityCount - 1];
        --entityCount;
    }
    dr_printf("entity.wasm: unloaded for %s (%d entities)", p.entity, entityCount);
}

static const char* const METHODS[] = { "preload", "unload", "clickDownOnEntity", "collisionWithEntity", "ping", "describe" };

int32_t entity_has(uint8_t* payload, int32_t length) DARKRIDE_EXPORT("entity_has");
int32_t entity_has(uint8_t* payload, int32_t length) {
    dr_entity_payload p;
    bool ok = dr_entity_payload_read(payload, length, &p);
    free(payload);
    if (!ok) {
        return 0;
    }
    for (size_t i = 0; i < sizeof(METHODS) / sizeof(METHODS[0]); ++i) {
        if (strcmp(METHODS[i], p.method) == 0) {
            return 1;
        }
    }
    return 0;
}

/* entity_call: the methods, by name; the entity's id is in the payload, the arguments after it. */
int64_t entity_call(uint8_t* payload, int32_t length) DARKRIDE_EXPORT("entity_call");
int64_t entity_call(uint8_t* payload, int32_t length) {
    dr_entity_payload p;
    bool ok = dr_entity_payload_read(payload, length, &p);
    if (!ok) {
        free(payload);
        return 0;
    }
    int i = indexOf(p.entity);
    if (i < 0) {
        free(payload);
        return 0;
    }
    int64_t result = 0;
    if (strcmp(p.method, "preload") == 0) {
        dr_printf("entity.wasm: preload %s", p.entity);
        setName(p.entity, "entity.wasm: loaded");
    } else if (strcmp(p.method, "unload") == 0) {
        dr_printf("entity.wasm: unload %s after %d clicks and %d collisions", p.entity, entities[i].clicks, entities[i].collisions);
    } else if (strcmp(p.method, "clickDownOnEntity") == 0) {
        ++entities[i].clicks;
        /* the colour cycles with the clicks */
        setColor(p.entity, (entities[i].clicks * 70) % 256, 120, 255 - (entities[i].clicks * 70) % 256);
        char name[96];
        snprintf(name, sizeof(name), "entity.wasm: %d clicks", entities[i].clicks);
        setName(p.entity, name);
    } else if (strcmp(p.method, "collisionWithEntity") == 0) {
        ++entities[i].collisions;
        char name[96];
        snprintf(name, sizeof(name), "entity.wasm: %d collisions", entities[i].collisions);
        setName(p.entity, name);
    } else if (strcmp(p.method, "ping") == 0) {
        /* a remote call is allowed only for the methods a module answers for remote callers: this one */
        char text[256];
        cbor_item head, first;
        char arg[128] = "";
        if (p.args.p && cr_next(&p.args, &head) && head.type == CBOR_ARRAY && head.n >= 1 && cr_next(&p.args, &first)) {
            if (first.type == CBOR_ARRAY && first.n >= 1 && cr_next(&p.args, &first)) {
                cr_text(&first, arg, sizeof(arg)); /* the QStringList of a remote call */
            } else {
                cr_text(&first, arg, sizeof(arg));
            }
        }
        snprintf(text, sizeof(text), "entity.wasm: pinged with \"%s\" by %s", arg, p.remoteCaller[0] ? p.remoteCaller : "a local script");
        setName(p.entity, text);
        cbor_writer w;
        cw_init(&w);
        cw_text(&w, "pong");
        result = dr_pack_result(&w);
        cw_free(&w);
    } else if (strcmp(p.method, "describe") == 0) {
        cbor_writer w;
        cw_init(&w);
        cw_map(&w, 2);
        cw_key(&w, "clicks");
        cw_int(&w, entities[i].clicks);
        cw_key(&w, "collisions");
        cw_int(&w, entities[i].collisions);
        result = dr_pack_result(&w);
        cw_free(&w);
    }
    free(payload);
    return result;
}
