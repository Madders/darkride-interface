/*
 *  orbit.c
 *  tools/wasm-sdk/examples/c
 *
 *  A script in C: on each frame it moves the avatar on a circle around where it started, adds an entity to
 *  orbit around, measures the distance to it with Vec3.distance, prints what it does, fires one timer, and
 *  stops itself after a while. Built with build.sh (wasi-sdk) into orbit.wasm; run it like any script
 *  (Running Scripts, or `interface --testScript orbit.wasm`).
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#define DARKRIDE_IMPLEMENTATION
#include "../../c/darkride.h"

#include <math.h>

static const double RADIUS = 2.0;        /* metres */
static const double TURN_SECONDS = 8.0;  /* one orbit */
static const double RUN_SECONDS = 12.0;  /* then the script stops itself */
static const int32_t TIMER_CALLBACK = 1;
static const int ADD_TRIES = 10;         /* seconds to wait for the world before giving up on the entity */

static int32_t Script, MyAvatar, Entities, Vec3;
static int32_t updateConnection;
static dr_vec3 center;
static char entityId[64];
static double elapsed;
static double angle;
static int frames;
static int setFailures;
static int addTries;

static bool readVec3Property(int32_t object, const char* property, dr_vec3* out) {
    dr_bytes bytes = dr_get_property(object, property);
    bool ok = dr_result_vec3(bytes, out);
    dr_bytes_free(bytes);
    return ok;
}

/* Adds the entity to orbit around: false when there is no world to add it to yet (the id comes back null). */
static bool addEntity(void) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 1);
    cw_map(&args, 6);
    cw_key(&args, "type");
    cw_text(&args, "Box");
    cw_key(&args, "name");
    cw_text(&args, "wasm-orbit");
    cw_key(&args, "position");
    dr_vec3 position = { center.x, center.y + 1.0, center.z };
    cw_vec3(&args, position);
    cw_key(&args, "dimensions");
    dr_vec3 dimensions = { 0.3, 0.3, 0.3 };
    cw_vec3(&args, dimensions);
    cw_key(&args, "color");
    cw_map(&args, 3);
    cw_key(&args, "red");
    cw_int(&args, 255);
    cw_key(&args, "green");
    cw_int(&args, 140);
    cw_key(&args, "blue");
    cw_int(&args, 0);
    cw_key(&args, "lifetime");
    cw_int(&args, 120);
    dr_bytes result = dr_call_args(Entities, "addEntity", &args);
    cw_free(&args);
    char message[256];
    if (dr_is_error(result, message, sizeof(message))) {
        dr_logf(DR_LOG_ERROR, "orbit: addEntity failed: %s", message);
        dr_bytes_free(result);
        return true;  /* refused for good: not worth another try */
    }
    if (dr_result_is_null(result)) {
        dr_bytes_free(result);
        return false;
    }
    dr_result_text(result, entityId, sizeof(entityId));
    dr_printf("orbit: added entity %s", entityId);
    dr_bytes_free(result);

    /* the distance from the avatar to the entity, through the Vec3 library */
    cw_init(&args);
    cw_array(&args, 2);
    cw_vec3(&args, center);
    cw_vec3(&args, position);
    result = dr_call_args(Vec3, "distance", &args);
    cw_free(&args);
    dr_printf("orbit: Vec3.distance to the entity = %.3f", dr_result_number(result));
    dr_bytes_free(result);
    return true;
}

/* A timer: the host calls on_callback(TIMER_CALLBACK) once, a second from now. */
static void startTimer(void) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_callback(&args, TIMER_CALLBACK);
    cw_int(&args, 1000);
    dr_bytes result = dr_call_args(Script, "setTimeout", &args);
    cw_free(&args);
    char message[256];
    if (dr_is_error(result, message, sizeof(message))) {
        dr_logf(DR_LOG_ERROR, "orbit: setTimeout failed: %s", message);
    }
    dr_bytes_free(result);
}

void script_start(void) DARKRIDE_EXPORT("script_start");
void script_start(void) {
    if (dr_version() != DARKRIDE_ABI_VERSION) {
        dr_logf(DR_LOG_ERROR, "orbit: host ABI %d, this module expects %d", dr_version(), DARKRIDE_ABI_VERSION);
        return;
    }
    dr_load_globals();
    Script = dr_global("Script");
    MyAvatar = dr_global("MyAvatar");
    Entities = dr_global("Entities");
    Vec3 = dr_global("Vec3");
    if (!Script || !MyAvatar || !Entities || !Vec3) {
        dr_logf(DR_LOG_ERROR, "orbit: missing globals (Script %d, MyAvatar %d, Entities %d, Vec3 %d)", Script, MyAvatar, Entities, Vec3);
        return;
    }
    if (!readVec3Property(MyAvatar, "position", &center)) {
        dr_logf(DR_LOG_ERROR, "orbit: cannot read MyAvatar.position");
        return;
    }
    dr_printf("orbit: starting at (%.2f, %.2f, %.2f), radius %.1f m, one turn in %.0f s", center.x, center.y, center.z, RADIUS, TURN_SECONDS);

    /* the entity is added a second from now, and each second after that until the world has loaded (a script can
       start before its world does, and an entity added before that gets no id) */
    startTimer();

    updateConnection = dr_connect_signal(Script, "update");
    if (!updateConnection) {
        dr_logf(DR_LOG_ERROR, "orbit: cannot connect Script.update");
    }
}

void on_signal(int32_t connection, const uint8_t* args, int32_t length) DARKRIDE_EXPORT("on_signal");
void on_signal(int32_t connection, const uint8_t* args, int32_t length) {
    /* update(deltaTime): the arguments are [deltaTime]; they are the module's to free */
    cbor_reader r = cr_from_memory(args, (size_t)length);
    cbor_item head, dt;
    double deltaTime = 0.0;
    if (cr_next(&r, &head) && head.type == CBOR_ARRAY && head.n >= 1 && cr_next(&r, &dt)) {
        deltaTime = cr_number(&dt);
    }
    free((void*)args);
    if (connection != updateConnection) {
        return;
    }
    elapsed += deltaTime;
    angle += deltaTime * (2.0 * M_PI / TURN_SECONDS);
    ++frames;

    dr_vec3 position = { center.x + RADIUS * cos(angle), center.y, center.z + RADIUS * sin(angle) };
    cbor_writer value;
    cw_init(&value);
    cw_vec3(&value, position);
    if (!dr_set_property(MyAvatar, "position", &value) && setFailures++ == 0) {
        dr_logf(DR_LOG_ERROR, "orbit: setting MyAvatar.position failed");
    }
    cw_free(&value);

    if (frames % 120 == 0) {
        dr_bytes stats = dr_call0(Script, "statistics");
        cbor_reader s = cr_from(stats);
        cbor_item calls;
        double count = cr_find(&s, "calls", &calls) ? cr_number(&calls) : 0.0;
        dr_bytes_free(stats);
        dr_printf("orbit: %.1f s, %d frames, %.1f Hz, %.0f host calls", elapsed, frames, frames / elapsed, count);
    }
    if (elapsed >= RUN_SECONDS) {
        dr_vec3 where;
        if (readVec3Property(MyAvatar, "position", &where)) {
            dr_printf("orbit: done after %.1f s and %d frames; the avatar is at (%.2f, %.2f, %.2f), %.2f m from the centre",
                      elapsed, frames, where.x, where.y, where.z,
                      sqrt((where.x - center.x) * (where.x - center.x) + (where.z - center.z) * (where.z - center.z)));
        }
        dr_disconnect(updateConnection);
        updateConnection = 0;
        dr_bytes result = dr_call0(Script, "stop");
        dr_bytes_free(result);
    }
}

void on_callback(int32_t callback, const uint8_t* args, int32_t length) DARKRIDE_EXPORT("on_callback");
void on_callback(int32_t callback, const uint8_t* args, int32_t length) {
    (void)length;
    free((void*)args);
    if (callback == TIMER_CALLBACK) {
        dr_printf("orbit: the timer fired after %.2f s", elapsed);
        if (!addEntity()) {
            if (++addTries < ADD_TRIES) {
                dr_print("orbit: no world to add the entity to yet; trying again in a second");
                startTimer();
            } else {
                dr_printf("orbit: no world after %d s; going on without the entity", ADD_TRIES);
            }
        }
    }
}

void script_stop(void) DARKRIDE_EXPORT("script_stop");
void script_stop(void) {
    if (entityId[0]) {
        cbor_writer args;
        cw_init(&args);
        cw_array(&args, 1);
        cw_text(&args, entityId);
        dr_bytes result = dr_call_args(Entities, "deleteEntity", &args);
        cw_free(&args);
        dr_bytes_free(result);
    }
    dr_print("orbit: bye");
    /* under --testScript the Test object exists: quit the client, as the test scripts do */
    int32_t Test = dr_global("Test");
    if (Test) {
        dr_bytes result = dr_call0(Test, "quit");
        dr_bytes_free(result);
    }
}
