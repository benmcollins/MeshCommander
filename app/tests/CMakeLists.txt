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

qt_add_executable(test_kvmcontroller
    test_kvmcontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kvmcontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kvmcontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kvmframebuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kvmframebuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mjpeg_mov_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mjpeg_mov_recorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ssh_tunnel_opener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ssh_tunnel_opener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sshtunnelhost.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sshtunnelhost.h
)
target_include_directories(test_kvmcontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_kvmcontroller PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    qumesh_kvm
    qumesh_redir
    qumesh_ssh
    qumesh_wsman
)
add_test(NAME test_kvmcontroller COMMAND test_kvmcontroller)

qt_add_executable(test_asciicast_recorder
    test_asciicast_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/asciicast_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/asciicast_recorder.h
)
target_include_directories(test_asciicast_recorder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_asciicast_recorder PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_asciicast_recorder COMMAND test_asciicast_recorder)

qt_add_executable(test_mjpeg_mov_recorder
    test_mjpeg_mov_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mjpeg_mov_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/mjpeg_mov_recorder.h
)
target_include_directories(test_mjpeg_mov_recorder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_mjpeg_mov_recorder PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
add_test(NAME test_mjpeg_mov_recorder COMMAND test_mjpeg_mov_recorder)

qt_add_executable(test_filename_formatter
    test_filename_formatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filename_formatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filename_formatter.h
)
target_include_directories(test_filename_formatter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_filename_formatter PRIVATE
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_filename_formatter COMMAND test_filename_formatter)

# QML engine smoke test (#251). Loads `Main.qml` headless and asserts
# no warning/critical/fatal QtMsgs land during construction — catches
# the class of runtime QML errors that compile clean and only blow up
# at first launch (e.g. the `Icon.qml` `implicitHeight is a read-only
# property` regression from PR #250). Links `qumesh_appcore` so the
# QML module + every C++ type the production app registers is on hand
# without re-declaring them. Brings its own no-op `Updater` impl
# because `qumesh_appcore` deliberately omits Updater (each consumer
# picks a platform-specific impl); the test wants the stub.
qt_add_executable(test_qml_engine_loads
    test_qml_engine_loads.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/updater.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/updater.h
)
target_link_libraries(test_qml_engine_loads PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Test
    qumesh_appcore
    qumesh_appcoreplugin
    qumesh_appcoreplugin_init
)
add_test(NAME test_qml_engine_loads COMMAND test_qml_engine_loads)
# Belt and braces: even though the test sets QT_QPA_PLATFORM via
# qputenv before constructing QGuiApplication, surface the same hint
# through the ctest environment so a runner inspecting `ctest -V`
# output sees the offscreen mode clearly.
set_tests_properties(test_qml_engine_loads PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# On Windows, run `windeployqt` against the test exe in a POST_BUILD
# step so every Qt DLL the test transitively imports — and every Qt
# plugin (qsvg image format, qoffscreen QPA, Qt Quick QML modules) —
# lands next to the .exe. Otherwise the loader can't start the binary
# under `ctest` and the test fails with zero captured output before
# main() even runs (the failure mode that drove #262). The other Qt
# tests in this dir get away with PATH alone because they don't pull
# in the QML stack and its plugins. On macOS/Linux this block is a
# no-op (no windeployqt; not needed). See #262.
if(WIN32)
    get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXE
        NAMES windeployqt windeployqt6
        PATHS "${_qt_bin}"
        NO_DEFAULT_PATH
    )
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET test_qml_engine_loads POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/../qml"
                "$<TARGET_FILE:test_qml_engine_loads>"
            COMMENT "Running windeployqt on test_qml_engine_loads.exe"
            VERBATIM
        )
    else()
        message(WARNING
            "windeployqt not found near Qt6::qmake (${_qt_bin}); "
            "test_qml_engine_loads will likely fail to load on Windows.")
    endif()
endif()

qt_add_executable(test_solcontroller
    test_solcontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/asciicast_recorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/asciicast_recorder.h
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

# BatchRunner unit test: drives the runner with a stub dispatcher so
# the WSMAN path is out of scope. Lives outside the qumesh_app_test
# helper because it only needs BatchRunner — no ConfigStore / model /
# migration plumbing.
qt_add_executable(test_batchrunner
    test_batchrunner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/batchrunner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/batchrunner.h
)
target_include_directories(test_batchrunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_batchrunner PRIVATE
    Qt6::Core
    Qt6::Test
    qumesh_wsman
)
add_test(NAME test_batchrunner COMMAND test_batchrunner)
