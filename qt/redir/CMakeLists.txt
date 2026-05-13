# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(meshcommander_redir STATIC
    src/redir_codec.cpp
    src/redir_client.cpp
    src/sol_session.cpp
    include/redir/redir_codec.h
    include/redir/redir_client.h
    include/redir/sol_session.h
)

target_include_directories(meshcommander_redir
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_redir
    PUBLIC
        Qt6::Core
        Qt6::Network
)

if(MSVC)
    target_compile_options(meshcommander_redir PRIVATE /W4)
else()
    target_compile_options(meshcommander_redir PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(qumesh-redir cli/main.cpp)
target_link_libraries(qumesh-redir PRIVATE
    Qt6::Core
    meshcommander_redir
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
