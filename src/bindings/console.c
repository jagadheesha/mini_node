#include "runtime.h"

#include <stdio.h>

static JSValue js_console_log(JSContext *context, JSValueConst this_val, int argc, JSValueConst *argv) {
    int index;
    (void)this_val;
    for (index = 0; index < argc; index++) {
        const char *value = JS_ToCString(context, argv[index]);
        if (!value) {
            return JS_EXCEPTION;
        }
        if (index > 0) {
            fputc(' ', stdout);
        }
        fputs(value, stdout);
        JS_FreeCString(context, value);
    }
    fputc('\n', stdout);
    fflush(stdout);
    return JS_UNDEFINED;
}

static JSValue js_performance_now(JSContext *context, JSValueConst this_val, int argc, JSValueConst *argv) {
    static uint64_t start = 0;
    uint64_t now;
    (void)context;
    (void)this_val;
    (void)argc;
    (void)argv;
    now = uv_hrtime();
    if (start == 0) {
        start = now;
    }
    return JS_NewFloat64(context, (double)(now - start) / 1000000.0);
}

void mini_install_console(JSContext *context) {
    JSValue global = JS_GetGlobalObject(context);
    JSValue console = JS_NewObject(context);
    JSValue performance = JS_NewObject(context);
    JS_SetPropertyStr(context, console, "log", JS_NewCFunction(context, js_console_log, "log", 0));
    JS_SetPropertyStr(context, performance, "now", JS_NewCFunction(context, js_performance_now, "now", 0));
    JS_SetPropertyStr(context, global, "console", console);
    JS_SetPropertyStr(context, global, "performance", performance);
    JS_FreeValue(context, global);
}