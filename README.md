# mini-node

`mini-node` is a small JavaScript runtime experiment built by embedding
[QuickJS](https://bellard.org/quickjs/) for JavaScript execution and
[libuv](https://libuv.org/) for timers, filesystem work, and the event loop.
The goal is to make the boundary between a JavaScript engine and native
asynchronous I/O easy to inspect.

> **Status:** This repository currently contains the project specification and
> package metadata. The native runtime described below is the implementation
> roadmap; the listed commands will become available as those files are added.

## Architecture

```text
JavaScript source
             |
             v
Native bindings (C) ---- JSRuntime / JSContext (QuickJS)
             |
             v
libuv loop, timers, and filesystem worker pool
```

The host owns one QuickJS runtime and context. Native functions are registered
on `globalThis`, retain JavaScript callbacks with `JS_DupValue()`, and release
those references with `JS_FreeValue()` after invocation. Once the initial script
has returned, the host runs `uv_run(loop, UV_RUN_DEFAULT)`. libuv callbacks
then re-enter QuickJS on the loop thread.

The runtime is intentionally single-threaded from JavaScript's perspective.
libuv may perform filesystem work in its worker pool, but JavaScript callbacks
must be dispatched back on the thread that owns the QuickJS context.

## Public API

### `console.log(...values)`

Prints primitive values and strings to standard output. Values should be
converted with QuickJS's value-to-string APIs and released after conversion.

### `performance.now()`

Returns a monotonic timestamp in milliseconds. It can be implemented from
`uv_hrtime()` without using wall-clock time, so elapsed durations are not
affected by system clock changes.

### `setTimeout(callback, delayMs)`

Schedules a callback on a `uv_timer_t`. The binding validates that the first
argument is callable and that the delay is non-negative, retains the callback
until the timer fires, then frees the timer and callback resources.

### `mini.readFile(path, callback)`

Reads a file without blocking the JavaScript thread. The callback follows the
shape `(error, data)`, where `data` is a byte array or string chosen by the
binding contract. Every `uv_fs_t` request must be cleaned up, including error
paths.

## Planned layout

```text
mini-node/
├── CMakeLists.txt
├── vendor/
│   ├── quickjs/
│   └── libuv/
├── src/
│   ├── main.c
│   ├── runtime.h
│   └── bindings/
│       ├── console.c
│       ├── timer.c
│       └── fs.c
└── examples/
        └── test-async.js
```

## Implementation plan

1. **Engine and evaluator:** initialize `JSRuntime` and `JSContext`, evaluate a
     script supplied on the command line, and dispose both objects on every exit
     path.
2. **Host bindings:** expose `console.log()` and `performance.now()` with
     argument validation and explicit QuickJS value ownership.
3. **Timers:** connect `setTimeout()` to `uv_timer_t`, retain callbacks while
     queued, and report JavaScript exceptions from `JS_Call()`.
4. **Filesystem:** implement `mini.readFile()` with `uv_fs_*` requests,
     callback error handling, and cleanup for open, read, and close operations.
5. **Verification:** run the example below and add focused tests for callback
     lifetime, timer ordering, missing files, and thrown callback exceptions.

## Build prerequisites

- A C compiler with C11 support
- CMake 3.16 or newer
- libuv development headers and library, unless vendored
- QuickJS source or a compatible development build

The current `package.json` is metadata for the repository and is not the build
system for the native runtime. Once `CMakeLists.txt` is present, the expected
build flow is:

```sh
cmake -S . -B build
cmake --build build
```

## Verification example

`examples/test-async.js` should contain:

```js
console.log("1: Main script starts");

setTimeout(() => {
    console.log("3: Timer callback executed");
}, 50);

mini.readFile("./package.json", (err, data) => {
    if (err) console.log("File error:", err);
    else console.log("4: Async file I/O finished, bytes:", data.length);
});

console.log("2: Main script finishes, entering event loop");
```

The first two lines must print synchronously. The timer and filesystem lines
may appear in either order because they are independent asynchronous tasks.
