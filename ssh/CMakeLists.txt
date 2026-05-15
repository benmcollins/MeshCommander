# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(qumesh_ssh STATIC
    src/ssh_session.cpp
    src/ssh_tunnel.cpp
    src/ssh_session_worker.h
    include/ssh/ssh_session.h
    include/ssh/ssh_tunnel.h
)

target_include_directories(qumesh_ssh
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(qumesh_ssh
    PUBLIC
        Qt6::Core
        Qt6::Network
        ssh
)

if(MSVC)
    target_compile_options(qumesh_ssh PRIVATE /W4)
else()
    target_compile_options(qumesh_ssh PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
