# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_executable(test_kvm_codec test_kvm_codec.cpp)
target_link_libraries(test_kvm_codec PRIVATE
    meshcommander_kvm
    Qt6::Test
)
add_test(NAME test_kvm_codec COMMAND test_kvm_codec)

qt_add_executable(test_kvm_session test_kvm_session.cpp)
target_link_libraries(test_kvm_session PRIVATE
    meshcommander_kvm
    Qt6::Network
    Qt6::Test
)
add_test(NAME test_kvm_session COMMAND test_kvm_session)
