#ifndef MINI_NODE_RUNTIME_H
#define MINI_NODE_RUNTIME_H

#include <quickjs.h>
#include <uv.h>

typedef struct MiniHost {
    JSRuntime *runtime;
    JSContext *context;
    uv_loop_t *loop;
} MiniHost;

MiniHost *mini_host(JSContext *context);
void mini_report_exception(JSContext *context);
int mini_call_callback(JSContext *context, JSValue callback, int argc, JSValueConst *argv);
void mini_install_console(JSContext *context);
void mini_install_timer(JSContext *context);
void mini_install_fs(JSContext *context);

#endif