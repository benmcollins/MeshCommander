# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_executable(test_ssh_session
    test_ssh_session.cpp
)
target_link_libraries(test_ssh_session PRIVATE
    Qt6::Core
    Qt6::Test
    qumesh_ssh
)
add_test(NAME test_ssh_session COMMAND test_ssh_session)

# Tunnel shutdown-under-load (#293). Spins up a minimal libssh
# server inside the test process — needs libssh's server.h symbols
# plus OpenSSL for the ephemeral host-key generator.
qt_add_executable(test_ssh_tunnel_shutdown
    test_ssh_tunnel_shutdown.cpp
)
target_link_libraries(test_ssh_tunnel_shutdown PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    qumesh_ssh
    ssh
    OpenSSL::Crypto
)
add_test(NAME test_ssh_tunnel_shutdown COMMAND test_ssh_tunnel_shutdown)
