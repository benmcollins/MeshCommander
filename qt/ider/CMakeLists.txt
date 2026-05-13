# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(meshcommander_ider STATIC
    include/ider/ider_codec.h
    include/ider/scsi.h
    include/ider/ider_session.h
    src/ider_codec.cpp
    src/scsi.cpp
    src/ider_session.cpp
)

target_include_directories(meshcommander_ider PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_ider PUBLIC
    Qt6::Core
    meshcommander_redir
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
