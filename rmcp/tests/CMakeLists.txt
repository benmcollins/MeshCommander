# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

function(qumesh_rmcp_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
        qumesh_rmcp
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qumesh_rmcp_test(test_pong_parser)
qumesh_rmcp_test(test_target_range)
