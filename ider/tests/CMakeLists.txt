# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_executable(test_ider_codec test_ider_codec.cpp)
target_link_libraries(test_ider_codec PRIVATE
    qumesh_ider
    Qt6::Test
)
add_test(NAME test_ider_codec COMMAND test_ider_codec)

qt_add_executable(test_ider_session test_ider_session.cpp)
target_link_libraries(test_ider_session PRIVATE
    qumesh_ider
    Qt6::Network
    Qt6::Test
)
add_test(NAME test_ider_session COMMAND test_ider_session)
