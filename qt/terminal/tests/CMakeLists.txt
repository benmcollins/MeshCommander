# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_executable(test_vt100parser test_vt100parser.cpp)
target_link_libraries(test_vt100parser PRIVATE
    qumesh_terminal
    Qt6::Test
)
add_test(NAME test_vt100parser COMMAND test_vt100parser)

qt_add_executable(test_terminalscreen test_terminalscreen.cpp)
target_link_libraries(test_terminalscreen PRIVATE
    qumesh_terminal
    Qt6::Test
)
add_test(NAME test_terminalscreen COMMAND test_terminalscreen)
