cmake_minimum_required(VERSION 3.20.0)
project(of_core_android VERSION 1.0.1 DESCRIPTION "OpenFiles Platform for Android")

include(configs/default)
configure_file(include/ofc_android/config.h.in ofc_android/config.h @ONLY)
include_directories(
        ${of_core_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${of_core_android_SOURCE_DIR}/include
)

set(SRCS
        src/console_android.c
        src/env_android.c
        src/event_android.c
        src/lock_android.c
        src/net_android.c
        src/process_android.c
        src/socket_android.c
        src/thread_android.c
        src/time_android.c
        src/waitset_android.c
        )

add_library(of_core_android STATIC ${SRCS})
target_link_libraries(of_core_android PUBLIC of_core_static)

set_target_properties(of_core_android PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(of_core_android PROPERTIES SOVERSION 1)

