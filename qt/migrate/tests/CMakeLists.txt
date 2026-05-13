# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

function(meshcommander_migrate_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        meshcommander_migrate
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

meshcommander_migrate_test(test_legacy_decrypt)

# These two seed a real leveldb to exercise the reader/orchestrator, so they
# need direct access to leveldb's headers.
meshcommander_migrate_test(test_chromium_storage)
target_link_libraries(test_chromium_storage PRIVATE leveldb)
meshcommander_migrate_test(test_migrate)
target_link_libraries(test_migrate PRIVATE leveldb)
