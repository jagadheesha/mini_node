#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>

MiniHost *mini_host(JSContext *context) {
    return (MiniHost *)JS_GetContextOpaque(context);
}

void mini_report_exception(JSContext *context) {
    JSValue exception = JS_GetException(context);
    const char *message = JS_ToCString(context, exception);
    if (message) {
        fprintf(stderr, "JavaScript exception: %s\n", message);
        JS_FreeCString(context, message);
    }
    JS_FreeValue(context, exception);
}

int mini_call_callback(JSContext *context, JSValue callback, int argc, JSValueConst *argv) {
    JSValue result = JS_Call(context, callback, JS_UNDEFINED, argc, argv);
    if (JS_IsException(result)) {
        mini_report_exception(context);
        JS_FreeValue(context, result);
        return -1;
    }
    JS_FreeValue(context, result);
    return 0;
}

static int evaluate_file(MiniHost *host, const char *path) {
    FILE *file = fopen(path, "rb");
    long length;
    size_t read_count;
    char *source;
    JSValue result;

    if (!file) {
        perror(path);
        return 1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "Could not determine the script size: %s\n", path);
        return 1;
    }
    source = malloc((size_t)length + 1);
    if (!source) {
        fclose(file);
        fprintf(stderr, "Out of memory while reading %s\n", path);
        return 1;
    }
    read_count = fread(source, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(source);
        fprintf(stderr, "Could not read %s\n", path);
        return 1;
    }
    source[length] = '\0';
    result = JS_Eval(host->context, source, (size_t)length, path, JS_EVAL_TYPE_GLOBAL);
    free(source);
    if (JS_IsException(result)) {
        mini_report_exception(host->context);
        JS_FreeValue(host->context, result);
        return 1;
    }
    JS_FreeValue(host->context, result);
    return 0;
}

int main(int argc, char **argv) {
    MiniHost host;
    int exit_code;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s script.js\n", argv[0]);
        return 2;
    }
    host.loop = uv_default_loop();
    host.runtime = JS_NewRuntime();
    host.context = host.runtime ? JS_NewContext(host.runtime) : NULL;
    if (!host.context) {
        fprintf(stderr, "Could not initialize QuickJS\n");
        JS_FreeRuntime(host.runtime);
        return 1;
    }
    JS_SetContextOpaque(host.context, &host);
    mini_install_console(host.context);
    mini_install_timer(host.context);
    mini_install_fs(host.context);
    exit_code = evaluate_file(&host, argv[1]);
    if (exit_code == 0) {
        uv_run(host.loop, UV_RUN_DEFAULT);
    }
    uv_loop_close(host.loop);
    JS_FreeContext(host.context);
    JS_FreeRuntime(host.runtime);
    return exit_code;
}