cmake_minimum_required(VERSION 3.16)
project(mini_node C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_path(QUICKJS_INCLUDE_DIR quickjs.h)
find_library(QUICKJS_LIBRARY NAMES quickjs)
find_path(LIBUV_INCLUDE_DIR uv.h)
find_library(LIBUV_LIBRARY NAMES uv libuv)

if(NOT QUICKJS_INCLUDE_DIR OR NOT QUICKJS_LIBRARY)
    message(FATAL_ERROR "QuickJS was not found. Set QUICKJS_INCLUDE_DIR and QUICKJS_LIBRARY.")
endif()

if(NOT LIBUV_INCLUDE_DIR OR NOT LIBUV_LIBRARY)
    message(FATAL_ERROR "libuv was not found. Set LIBUV_INCLUDE_DIR and LIBUV_LIBRARY.")
endif()

add_executable(mini-node
    src/main.c
    src/bindings/console.c
    src/bindings/timer.c
    src/bindings/fs.c
)

target_include_directories(mini-node PRIVATE
    src
    ${QUICKJS_INCLUDE_DIR}
    ${LIBUV_INCLUDE_DIR}
)

target_link_libraries(mini-node PRIVATE ${QUICKJS_LIBRARY} ${LIBUV_LIBRARY})

if(WIN32)
    target_link_libraries(mini-node PRIVATE ws2_32 iphlpapi userenv psapi)
endif()