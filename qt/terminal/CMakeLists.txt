# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(meshcommander_terminal STATIC
    include/terminal/vt100parser.h
    include/terminal/terminalscreen.h
    src/vt100parser.cpp
    src/terminalscreen.cpp
)

target_include_directories(meshcommander_terminal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_terminal PUBLIC
    Qt6::Core
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
