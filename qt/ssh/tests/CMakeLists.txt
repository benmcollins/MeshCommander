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
