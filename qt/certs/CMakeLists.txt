# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(meshcommander_certs STATIC
    include/certs/cert_entry.h
    include/certs/cert_parser.h
    include/certs/cert_store.h
    src/cert_entry.cpp
    src/cert_parser.cpp
    src/cert_store.cpp
)

target_include_directories(meshcommander_certs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_certs PUBLIC
    Qt6::Core
    Qt6::Network
    OpenSSL::Crypto
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
