#include "runtime.h"

#include <stdlib.h>

typedef struct MiniReadFile {
    JSContext *context;
    JSValue callback;
    uv_fs_t open_request;
    uv_fs_t read_request;
    uv_fs_t close_request;
    uv_file file;
    char *path;
    char *buffer;
    size_t length;
} MiniReadFile;

static void read_file_cleanup(MiniReadFile *request) {
    JS_FreeValue(request->context, request->callback);
    free(request->path);
    free(request->buffer);
    free(request);
}

static void read_file_callback(MiniReadFile *request, int error, const char *data, size_t length) {
    JSValue argv[2];
    if (error) {
        argv[0] = JS_NewString(request->context, uv_strerror(error));
        argv[1] = JS_NULL;
    } else {
        argv[0] = JS_NULL;
        argv[1] = JS_NewStringLen(request->context, data, length);
    }
    mini_call_callback(request->context, request->callback, 2, argv);
    JS_FreeValue(request->context, argv[0]);
    JS_FreeValue(request->context, argv[1]);
}

static void close_done(uv_fs_t *close_request) {
    MiniReadFile *request = (MiniReadFile *)close_request->data;
    uv_fs_req_cleanup(close_request);
    read_file_cleanup(request);
}

static void read_done(uv_fs_t *read_request) {
    MiniReadFile *request = (MiniReadFile *)read_request->data;
    ssize_t result = read_request->result;
    uv_buf_t buffer = uv_buf_init(request->buffer, (unsigned int)request->length);
    uv_fs_req_cleanup(read_request);
    if (result < 0) {
        read_file_callback(request, (int)result, NULL, 0);
    } else {
        read_file_callback(request, 0, request->buffer, (size_t)result);
    }
    uv_fs_close(mini_host(request->context)->loop, &request->close_request, request->file, close_done);
    request->close_request.data = request;
    (void)buffer;
}

static void opened(uv_fs_t *open_request) {
    MiniReadFile *request = (MiniReadFile *)open_request->data;
    uv_stat_t stat;
    int result;
    request->file = (uv_file)open_request->result;
    uv_fs_req_cleanup(open_request);
    if (request->file < 0) {
        read_file_callback(request, (int)request->file, NULL, 0);
        read_file_cleanup(request);
        return;
    }
    result = uv_fs_fstat(NULL, &request->read_request, request->file, &stat, NULL);
    uv_fs_req_cleanup(&request->read_request);
    if (result < 0 || stat.st_size < 0) {
        read_file_callback(request, result < 0 ? result : UV_EFBIG, NULL, 0);
        uv_fs_close(mini_host(request->context)->loop, &request->close_request, request->file, close_done);
        request->close_request.data = request;
        return;
    }
    request->length = (size_t)stat.st_size;
    request->buffer = request->length ? malloc(request->length) : NULL;
    if (request->length && !request->buffer) {
        read_file_callback(request, UV_ENOMEM, NULL, 0);
        uv_fs_close(mini_host(request->context)->loop, &request->close_request, request->file, close_done);
        request->close_request.data = request;
        return;
    }
    request->read_request.data = request;
    uv_fs_read(mini_host(request->context)->loop, &request->read_request, request->file,
               request->buffer ? &(uv_buf_t){request->buffer, (unsigned int)request->length} : NULL,
               request->length ? 1 : 0, 0, read_done);
}

static JSValue js_read_file(JSContext *context, JSValueConst this_val, int argc, JSValueConst *argv) {
    MiniReadFile *request;
    const char *path;
    (void)this_val;
    if (argc < 2 || !JS_IsFunction(context, argv[1])) {
        return JS_ThrowTypeError(context, "mini.readFile requires a path and callback");
    }
    path = JS_ToCString(context, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    request = calloc(1, sizeof(*request));
    if (!request) {
        JS_FreeCString(context, path);
        return JS_ThrowOutOfMemory(context);
    }
    request->context = context;
    request->callback = JS_DupValue(context, argv[1]);
    request->path = _strdup(path);
    JS_FreeCString(context, path);
    request->open_request.data = request;
    if (uv_fs_open(mini_host(context)->loop, &request->open_request, request->path, UV_FS_O_RDONLY, 0, opened) < 0) {
        uv_fs_req_cleanup(&request->open_request);
        read_file_cleanup(request);
        return JS_ThrowInternalError(context, "could not queue file read");
    }
    return JS_UNDEFINED;
}

void mini_install_fs(JSContext *context) {
    JSValue global = JS_GetGlobalObject(context);
    JSValue mini = JS_NewObject(context);
    JS_SetPropertyStr(context, mini, "readFile", JS_NewCFunction(context, js_read_file, "readFile", 2));
    JS_SetPropertyStr(context, global, "mini", mini);
    JS_FreeValue(context, global);
}