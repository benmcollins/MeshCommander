# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(qumesh_rmcp STATIC
    src/pong.cpp
    src/target_range.cpp
    src/scanner.cpp
    include/rmcp/pong.h
    include/rmcp/target_range.h
    include/rmcp/scanner.h
)

target_include_directories(qumesh_rmcp
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(qumesh_rmcp
    PUBLIC
        Qt6::Core
        Qt6::Network
)

if(MSVC)
    target_compile_options(qumesh_rmcp PRIVATE /W4)
else()
    target_compile_options(qumesh_rmcp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
