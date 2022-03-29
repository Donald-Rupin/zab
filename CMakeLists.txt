#  MMM"""AMV       db      `7MM"""Yp,
#  M'   AMV       ;MM:       MM    Yb
#  '   AMV       ,V^MM.      MM    dP
#     AMV       ,M  `MM      MM"""bg.
#    AMV   ,    AbmmmqMA     MM    `Y
#   AMV   ,M   A'     VML    MM    ,9
#  AMVmmmmMM .AMA.   .AMMA..JMMmmmd9
cmake_minimum_required(VERSION 3.7...3.18 FATAL_ERROR)


project(ZAB, CXX)

set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -Og -g")
set(CMAKE_CXX_FLAGS_RELEASE "-DRELEASE -O3 -g")

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -fuse-linker-plugin -D_FILE_OFFSET_BITS=64")

enable_testing()

add_custom_target(liburing
                   COMMAND make
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/liburing
)

add_library(zab STATIC 
    src/event_loop.cpp
    src/engine.cpp
    src/signal_handler.cpp
    src/network_operation.cpp
    src/tcp_networking.cpp
    src/timer_service.cpp
    src/pause.cpp
    )

target_compile_options(zab PUBLIC
    -fcoroutines
    -pthread
    -Wall
    -Wextra
    -g
  )

target_include_directories(zab PUBLIC
    includes    
    liburing/src/include
  )


add_dependencies(zab liburing)

macro(add_zab_test test)

    message(STATUS "Adding test ${test}")

    add_executable(${test} test/${test}.cpp)

    target_compile_options(${test} PUBLIC
        -fcoroutines
        -pthread
        -Wall
        -Wextra
    )

    target_include_directories(${test} PUBLIC
        includes
        libs
        liburing/src/include
    )

    target_link_libraries(
        ${test} PUBLIC
         zab -lpthread -latomic uring
    )

    target_link_directories(
        ${test} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/liburing/src
    )

    add_dependencies(${test} liburing)

    add_test(
        NAME ${test}
        COMMAND ${test} 
    )

endmacro()

message(STATUS ${ZAB_NO_TESTS})   

if(NOT DEFINED ZAB_NO_TESTS)

    message(STATUS "COMPILING TESTS")   

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
    add_zab_test(test-observable)
    add_zab_test(test-file_io)
    add_zab_test(test-networking)
endif()

macro(add_zab_example example)

    message(STATUS "Adding example ${example}")

    add_executable(${example} example/${example}.cpp)

    target_compile_options(${example} PUBLIC
        -fcoroutines
        -pthread
        -Wall
        -Wextra
    )

    target_include_directories(${example} PUBLIC
        includes
        libs
        liburing/src/include
    )

    target_link_libraries(
        ${example} PUBLIC
         zab -lpthread -latomic uring
    )

    target_link_directories(
        ${example} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/liburing/src
    )


    add_dependencies(${example} liburing)

endmacro()

if(NOT DEFINED ZAB_NO_EXAMPLES)

    message(STATUS "COMPILING EXAMPLES")   

    add_zab_example(echo_server)
    add_zab_example(logging_echo_server)
endif()