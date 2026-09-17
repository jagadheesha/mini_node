#include "runtime.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct MiniTimer {
    uv_timer_t handle;
    JSContext *context;
    JSValue callback;
} MiniTimer;

static void timer_closed(uv_handle_t *handle) {
    MiniTimer *timer = (MiniTimer *)handle->data;
    JS_FreeValue(timer->context, timer->callback);
    free(timer);
}

static void timer_fired(uv_timer_t *handle) {
    MiniTimer *timer = (MiniTimer *)handle->data;
    mini_call_callback(timer->context, timer->callback, 0, NULL);
    uv_close((uv_handle_t *)handle, timer_closed);
}

static JSValue js_set_timeout(JSContext *context, JSValueConst this_val, int argc, JSValueConst *argv) {
    MiniTimer *timer;
    int64_t delay;
    int result;
    (void)this_val;
    if (argc < 2 || !JS_IsFunction(context, argv[0])) {
        return JS_ThrowTypeError(context, "setTimeout requires a callback and delay");
    }
    if (JS_ToInt64(context, &delay, argv[1]) < 0 || delay < 0) {
        return JS_ThrowRangeError(context, "delay must be a non-negative integer");
    }
    timer = calloc(1, sizeof(*timer));
    if (!timer) {
        return JS_ThrowOutOfMemory(context);
    }
    timer->context = context;
    timer->callback = JS_DupValue(context, argv[0]);
    timer->handle.data = timer;
    result = uv_timer_init(mini_host(context)->loop, &timer->handle);
    if (result == 0) {
        result = uv_timer_start(&timer->handle, timer_fired, (uint64_t)delay, 0);
    }
    if (result != 0) {
        JS_FreeValue(context, timer->callback);
        free(timer);
        return JS_ThrowInternalError(context, "could not start timer: %s", uv_strerror(result));
    }
    return JS_UNDEFINED;
}

void mini_install_timer(JSContext *context) {
    JSValue global = JS_GetGlobalObject(context);
    JS_SetPropertyStr(context, global, "setTimeout", JS_NewCFunction(context, js_set_timeout, "setTimeout", 2));
    JS_FreeValue(context, global);
}