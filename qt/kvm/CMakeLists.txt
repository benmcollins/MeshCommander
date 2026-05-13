# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(meshcommander_kvm STATIC
    include/kvm/kvm_codec.h
    include/kvm/kvm_session.h
    src/kvm_codec.cpp
    src/kvm_session.cpp
)

target_include_directories(meshcommander_kvm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_kvm PUBLIC
    Qt6::Core
    Qt6::Gui
    meshcommander_redir
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
