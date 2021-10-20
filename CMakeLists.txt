#  MMM"""AMV       db      `7MM"""Yp,
#  M'   AMV       ;MM:       MM    Yb
#  '   AMV       ,V^MM.      MM    dP
#     AMV       ,M  `MM      MM"""bg.
#    AMV   ,    AbmmmqMA     MM    `Y
#   AMV   ,M   A'     VML    MM    ,9
#  AMVmmmmMM .AMA.   .AMMA..JMMmmmd9

cmake_minimum_required(VERSION 3.7...3.18 FATAL_ERROR)

project(ZAB, CXX)

set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-DRELEASE -O3 -g")

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -fuse-linker-plugin -D_FILE_OFFSET_BITS=64")

enable_testing()

add_library(zab STATIC 
    src/event_loop.cpp
    src/engine.cpp
    src/signal_handler.cpp
    src/descriptor_notifications.cpp
    src/file_io_overlay.cpp
    src/network_overlay.cpp
    src/tcp_stream.cpp
    src/timer_service.cpp
    )

target_compile_options(zab PUBLIC
    -fcoroutines
    -pthread
    -Wall
    -Wextra
  )

target_include_directories(zab PUBLIC
    includes
    libs
  )


macro(add_zab_test test)

    message(STATUS "Adding test ${test}")

    add_executable(${test} test/${test}.cpp)

    target_compile_options(${test} PUBLIC
        -fcoroutines
        -g
        -pthread
        -Wall
        -Wextra
    )

    target_include_directories(${test} PUBLIC
        includes
        libs
    )

    target_link_libraries(
        ${test} PUBLIC
         zab -lpthread -latomic
    )

    add_test(
        NAME ${test}
        COMMAND ${test} 
    )

endmacro()

add_zab_test(test-engine_enabled)
add_zab_test(test-engine_awaitable)
add_zab_test(test-signal_handler)
add_zab_test(test-pause_token)
add_zab_test(test-async_latch)
add_zab_test(test-wait_for)
add_zab_test(test-for_each)
add_zab_test(test-first_of)
add_zab_test(test-async_mutex)
add_zab_test(test-async_semaphore)
add_zab_test(test-async_barrier)
add_zab_test(test-defer_block_promise)
add_zab_test(test-visitor_promise)
add_zab_test(test-file_io_overlay)
add_zab_test(test-network_overlay)

macro(add_zab_example example)

    message(STATUS "Adding example ${example}")

    add_executable(${example} example/${example}.cpp)

    target_compile_options(${example} PUBLIC
        -fcoroutines
        -g
        -pthread
        -Wall
        -Wextra
    )

    target_include_directories(${example} PUBLIC
        includes
        libs
    )

    target_link_libraries(
        ${example} PUBLIC
         zab -lpthread -latomic
    )

endmacro()


add_zab_example(echo_server)
add_zab_example(logging_echo_server)