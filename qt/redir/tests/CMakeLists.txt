# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

function(meshcommander_redir_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
        meshcommander_redir
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

meshcommander_redir_test(test_redir_codec)
meshcommander_redir_test(test_redir_client)
meshcommander_redir_test(test_sol_session)
meshcommander_redir_test(test_redir_tls)
target_link_libraries(test_redir_tls PRIVATE OpenSSL::Crypto)
