# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_executable(test_cert_store test_cert_store.cpp)
target_link_libraries(test_cert_store PRIVATE
    qumesh_certs
    Qt6::Test
    OpenSSL::Crypto
)
add_test(NAME test_cert_store COMMAND test_cert_store)
