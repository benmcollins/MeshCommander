# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(qumesh_wsman STATIC
    src/soap_envelope.cpp
    src/wsman_client.cpp
    src/operations.cpp
    src/local_amt_probe.cpp
    src/setupbin.cpp
    src/cert_request_builder.cpp
    include/wsman/soap_envelope.h
    include/wsman/wsman_client.h
    include/wsman/operations.h
    include/wsman/local_amt_probe.h
    include/wsman/setupbin.h
    include/wsman/cert_request_builder.h
)

target_include_directories(qumesh_wsman
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(qumesh_wsman
    PUBLIC
        Qt6::Core
        Qt6::Network
        OpenSSL::Crypto
)

if(MSVC)
    target_compile_options(qumesh_wsman PRIVATE /W4)
else()
    target_compile_options(qumesh_wsman PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(qumesh-wsman
    cli/main.cpp
)
target_link_libraries(qumesh-wsman PRIVATE
    Qt6::Core
    qumesh_wsman
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
