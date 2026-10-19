/*
 *  callbacks.c
 *  tools/wasm-sdk/examples/c
 *
 *  A script in C that uses the parts of the API that take a list or a function of the script: a ray pick told
 *  to ignore an entity (Picks.setIgnoreItemsVector), an animation state handler (MyAvatar.addAnimationStateCallback)
 *  that the avatar's animation calls on each update, and a controller mapping whose route ends in a function of
 *  the module (the mapping builders' fromValue and toValue). Each step prints a line starting "callbacks:"; the
 *  script cleans up and stops itself after five seconds. Built with build.sh (wasi-sdk) into callbacks.wasm.
 *
 *  Distributed under the Apache License, Version 2.0.
 *  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
 */

#define DARKRIDE_IMPLEMENTATION
#include "../../c/darkride.h"

/* the module's callback ids: the host calls on_callback with one of these */
enum { ANIMATION_STATE = 1, ROUTE, DONE };

static const char* MAPPING_NAME = "wasm-callbacks-example";
static const int PICK_TYPE_RAY = 0;  /* PickType.Ray */

static int32_t Script, Picks, MyAvatar, Controller, Entities;
static double pick;
static double stateHandler;
static int stateCalls, routeCalls;
static char entityId[64];

/* Prints a refused call and says whether the call went through. */
static bool succeeded(dr_bytes result, const char* what) {
    char message[256];
    if (dr_is_error(result, message, sizeof(message))) {
        dr_printf("callbacks: %s refused: %s", what, message);
        return false;
    }
    return true;
}

static void addEntity(void) {
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 1);
    cw_map(&args, 4);
    cw_key(&args, "type");
    cw_text(&args, "Box");
    cw_key(&args, "name");
    cw_text(&args, "wasm-callbacks");
    cw_key(&args, "position");
    dr_vec3 position = { 0.0, 2.0, -3.0 };
    cw_vec3(&args, position);
    cw_key(&args, "lifetime");
    cw_int(&args, 60);
    dr_bytes result = dr_call_args(Entities, "addEntity", &args);
    cw_free(&args);
    if (succeeded(result, "Entities.addEntity")) {
        dr_result_text(result, entityId, sizeof(entityId));
    }
    dr_bytes_free(result);
}

static void startPick(void) {
    dr_bytes filter = dr_get_property(Picks, "PICK_ENTITIES");
    double entities = dr_result_number(filter);
    dr_bytes_free(filter);

    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_int(&args, PICK_TYPE_RAY);
    cw_map(&args, 3);
    cw_key(&args, "joint");
    cw_text(&args, "Mouse");
    cw_key(&args, "filter");
    cw_int(&args, (int64_t)entities);
    cw_key(&args, "enabled");
    cw_bool(&args, true);
    dr_bytes result = dr_call_args(Picks, "createPick", &args);
    cw_free(&args);
    if (!succeeded(result, "Picks.createPick")) {
        dr_bytes_free(result);
        return;
    }
    pick = dr_result_number(result);
    dr_bytes_free(result);

    /* the entity is to be ignored, nothing in particular included: the lists as lists */
    cw_init(&args);
    cw_array(&args, 2);
    cw_int(&args, (int64_t)pick);
    cw_array(&args, 1);
    cw_text(&args, entityId);
    result = dr_call_args(Picks, "setIgnoreItemsVector", &args);
    cw_free(&args);
    bool ignored = succeeded(result, "Picks.setIgnoreItemsVector");
    dr_bytes_free(result);

    cw_init(&args);
    cw_array(&args, 2);
    cw_int(&args, (int64_t)pick);
    cw_array(&args, 0);
    result = dr_call_args(Picks, "setIncludeItemsVector", &args);
    cw_free(&args);
    bool included = succeeded(result, "Picks.setIncludeItemsVector");
    dr_bytes_free(result);
    dr_printf("callbacks: pick %.0f made, ignoring the entity %s, including %s", pick, ignored ? "set" : "not set",
              included ? "everything" : "(not set)");
}

static void startAnimationStateHandler(void) {
    /* the handler is the module's function ANIMATION_STATE; it asks for one of the animation's variables */
    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_callback(&args, ANIMATION_STATE);
    cw_array(&args, 1);
    cw_text(&args, "isMovingForward");
    dr_bytes result = dr_call_args(MyAvatar, "addAnimationStateCallback", &args);
    cw_free(&args);
    if (succeeded(result, "MyAvatar.addAnimationStateCallback")) {
        stateHandler = dr_result_number(result);
        dr_printf("callbacks: animation state handler %.0f added", stateHandler);
    }
    dr_bytes_free(result);
}

static void startMapping(void) {
    /* Controller.Standard.LX, the input the route reads */
    dr_bytes standard = dr_get_property(Controller, "Standard");
    cbor_reader r = cr_from(standard);
    cbor_item lx;
    double input = cr_find(&r, "LX", &lx) ? cr_number(&lx) : 0.0;
    dr_bytes_free(standard);

    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 1);
    cw_text(&args, MAPPING_NAME);
    dr_bytes result = dr_call_args(Controller, "newMapping", &args);
    cw_free(&args);
    int32_t mapping = dr_result_object(result);
    dr_bytes_free(result);
    if (!mapping) {
        dr_print("callbacks: Controller.newMapping gave no mapping");
        return;
    }

    /* mapping.from(Controller.Standard.LX).to(function): the builders' plain-value forms */
    cw_init(&args);
    cw_array(&args, 1);
    cw_int(&args, (int64_t)input);
    result = dr_call_args(mapping, "fromValue", &args);
    cw_free(&args);
    int32_t route = dr_result_object(result);
    dr_bytes_free(result);
    if (!route) {
        dr_print("callbacks: the mapping's fromValue gave no route");
        return;
    }
    cw_init(&args);
    cw_array(&args, 1);
    cw_callback(&args, ROUTE);
    result = dr_call_args(route, "toValue", &args);
    cw_free(&args);
    bool routed = succeeded(result, "the route's toValue");
    dr_bytes_free(result);

    cw_init(&args);
    cw_array(&args, 1);
    cw_text(&args, MAPPING_NAME);
    result = dr_call_args(Controller, "enableMapping", &args);
    cw_free(&args);
    bool enabled = succeeded(result, "Controller.enableMapping");
    dr_bytes_free(result);
    dr_printf("callbacks: mapping %s from input %.0f to the module, %s", routed ? "routed" : "not routed", input,
              enabled ? "enabled" : "not enabled");
}

void script_start(void) DARKRIDE_EXPORT("script_start");
void script_start(void) {
    dr_load_globals();
    Script = dr_global("Script");
    Picks = dr_global("Picks");
    MyAvatar = dr_global("MyAvatar");
    Controller = dr_global("Controller");
    Entities = dr_global("Entities");
    if (!Script || !Picks || !MyAvatar || !Controller || !Entities) {
        dr_logf(DR_LOG_ERROR, "callbacks: missing globals (Script %d, Picks %d, MyAvatar %d, Controller %d, Entities %d)",
                Script, Picks, MyAvatar, Controller, Entities);
        return;
    }
    addEntity();
    startPick();
    startAnimationStateHandler();
    startMapping();

    cbor_writer args;
    cw_init(&args);
    cw_array(&args, 2);
    cw_callback(&args, DONE);
    cw_int(&args, 5000);
    dr_bytes result = dr_call_args(Script, "setTimeout", &args);
    cw_free(&args);
    dr_bytes_free(result);
}

static void finish(void) {
    dr_printf("callbacks: in five seconds, the animation state handler was called %d times and the route %d times",
              stateCalls, routeCalls);
    cbor_writer args;
    dr_bytes result;
    if (stateHandler) {
        cw_init(&args);
        cw_array(&args, 1);
        cw_int(&args, (int64_t)stateHandler);
        result = dr_call_args(MyAvatar, "removeAnimationStateCallback", &args);
        cw_free(&args);
        dr_bytes_free(result);
    }
    cw_init(&args);
    cw_array(&args, 1);
    cw_text(&args, MAPPING_NAME);
    result = dr_call_args(Controller, "disableMapping", &args);
    cw_free(&args);
    dr_bytes_free(result);
    if (pick) {
        cw_init(&args);
        cw_array(&args, 1);
        cw_int(&args, (int64_t)pick);
        result = dr_call_args(Picks, "removePick", &args);
        cw_free(&args);
        dr_bytes_free(result);
    }
    if (entityId[0]) {
        cw_init(&args);
        cw_array(&args, 1);
        cw_text(&args, entityId);
        result = dr_call_args(Entities, "deleteEntity", &args);
        cw_free(&args);
        dr_bytes_free(result);
    }
    result = dr_call0(Script, "stop");
    dr_bytes_free(result);
}

void on_callback(int32_t callback, const uint8_t* payload, int32_t length) DARKRIDE_EXPORT("on_callback");
void on_callback(int32_t callback, const uint8_t* payload, int32_t length) {
    (void)length;
    switch (callback) {
        case ANIMATION_STATE:
            /* [variables]: the animation's state; a handler that answers nothing changes nothing */
            ++stateCalls;
            break;
        case ROUTE:
            /* [value]: the input's value, each time the mapping runs */
            ++routeCalls;
            break;
        case DONE:
            finish();
            break;
        default:
            break;
    }
    free((void*)payload);
}

void script_stop(void) DARKRIDE_EXPORT("script_stop");
void script_stop(void) {
    dr_print("callbacks: bye");
    int32_t Test = dr_global("Test");
    if (Test) {
        dr_bytes result = dr_call0(Test, "quit");
        dr_bytes_free(result);
    }
}
