# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

function(qumesh_app_test name)
    qt_add_executable(${name}
        ${name}.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/configstore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/configstore.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/computermodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/computermodel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/secret_codec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/secret_codec.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/migrationcontroller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/migrationcontroller.h
    )
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Qml
        Qt6::Test
        qumesh_migrate
        qumesh_wsman
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qt_add_executable(test_smoke test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE Qt6::Core Qt6::Test)
add_test(NAME smoke COMMAND test_smoke)

qumesh_app_test(test_configstore)
qumesh_app_test(test_computermodel)
qumesh_app_test(test_migrationcontroller)
# test_migrationcontroller also seeds a real leveldb fixture.
target_link_libraries(test_migrationcontroller PRIVATE leveldb)

qt_add_executable(test_solcontroller
    test_solcontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/solcontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/solcontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ssh_tunnel_opener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ssh_tunnel_opener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sshtunnelhost.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sshtunnelhost.h
)
target_include_directories(test_solcontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_solcontroller PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    qumesh_redir
    qumesh_ssh
    qumesh_terminal
    qumesh_wsman
)
add_test(NAME test_solcontroller COMMAND test_solcontroller)
