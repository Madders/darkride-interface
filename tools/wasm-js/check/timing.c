/* Times JavaScript in native QuickJS-NG, for tools/wasm-js/check-patches.sh. The engine is compiled in (this file
   includes quickjs.c), so the same harness times the unpatched and the patched source.

     timing <cases file>

   Each line of the cases file is name=iterations=source, the source a function of one argument, the number of
   iterations; empty lines and lines starting with # are skipped. A case runs seven times and the fastest run is
   printed as "name microseconds-per-iteration result"; the result must be the same on both sources.

   Two native functions are defined for the cases, both of two arguments, returning undefined: nativeNothing, a plain
   C function, and nativeDataNothing, a C function with data (JS_NewCFunctionData), as the JavaScript guest binds
   the engine's methods. */
#include "quickjs.c"
#include <time.h>

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static JSValue native_nothing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

static JSValue native_data_nothing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
                                   int magic, JSValueConst *data)
{
    return JS_UNDEFINED;
}

static void print_exception(JSContext *ctx, const char *name)
{
    JSValue exception = JS_GetException(ctx);
    const char *text = JS_ToCString(ctx, exception);
    fprintf(stderr, "%s: %s\n", name, text ? text : "an exception");
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, exception);
}

int main(int argc, char **argv)
{
    static char line[65536];
    JSRuntime *rt;
    JSContext *ctx;
    JSValue global, data;
    FILE *file;
    int failed = 0;

    if (argc != 2 || !(file = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: %s <cases file>\n", argv[0]);
        return 2;
    }
    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "nativeNothing", JS_NewCFunction(ctx, native_nothing, "nativeNothing", 2));
    data = JS_NewInt32(ctx, 7);
    JS_SetPropertyStr(ctx, global, "nativeDataNothing",
                      JS_NewCFunctionData(ctx, native_data_nothing, 2, 0, 1, &data));
    JS_FreeValue(ctx, global);

    while (fgets(line, sizeof(line), file)) {
        char *name = line, *iterations, *source;
        JSValue function;
        double best = 1e30, result = 0;
        int n, round;

        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == 0 || line[0] == '#')
            continue;
        if (!(iterations = strchr(name, '=')) || !(source = strchr(iterations + 1, '='))) {
            fprintf(stderr, "not name=iterations=source: %s\n", line);
            failed = 1;
            continue;
        }
        *iterations++ = 0;
        *source++ = 0;
        n = atoi(iterations);
        function = JS_Eval(ctx, source, strlen(source), name, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(function)) {
            print_exception(ctx, name);
            failed = 1;
            continue;
        }
        for (round = 0; round < 7; round++) {
            JSValue arg = JS_NewInt32(ctx, n), value;
            double start = now(), elapsed;
            value = JS_Call(ctx, function, JS_UNDEFINED, 1, &arg);
            elapsed = now() - start;
            if (JS_IsException(value)) {
                print_exception(ctx, name);
                failed = 1;
                break;
            }
            JS_ToFloat64(ctx, &result, value);
            JS_FreeValue(ctx, value);
            if (elapsed < best)
                best = elapsed;
        }
        if (round == 7)
            printf("%s %.4f %.17g\n", name, best * 1e6 / n, result);
        JS_FreeValue(ctx, function);
    }
    fclose(file);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return failed;
}
