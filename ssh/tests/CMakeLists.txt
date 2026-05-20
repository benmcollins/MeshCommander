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
#
# POSIX-only for now: vcpkg's libssh on Windows builds the server API
# but its in-process handshake against an ephemeral RSA-2048 host key
# hangs the client's auth step, so the test would always time out at
# 8 s on the windows-latest CI runner. The Qt-side concurrency we're
# guarding against is platform-agnostic — exercising it on macOS +
# Linux is enough to catch a regression. If the libssh-on-Windows
# server path becomes load-bearing somewhere else (it currently
# isn't), revisit the host-key algorithm + libssh-options matrix.
if(NOT WIN32)
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
endif()
