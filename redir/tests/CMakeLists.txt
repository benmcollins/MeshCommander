# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

function(qumesh_redir_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
        qumesh_redir
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qumesh_redir_test(test_redir_codec)
qumesh_redir_test(test_redir_client)
qumesh_redir_test(test_sol_session)
qumesh_redir_test(test_redir_tls)
target_link_libraries(test_redir_tls PRIVATE OpenSSL::Crypto)
