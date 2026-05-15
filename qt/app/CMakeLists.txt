# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_executable(qumesh
    src/main.cpp
    src/configstore.cpp
    src/configstore.h
    src/secret_codec.cpp
    src/secret_codec.h
    src/ssh_tunnel_opener.cpp
    src/ssh_tunnel_opener.h
    src/sshtunnelhost.cpp
    src/sshtunnelhost.h
    src/computermodel.cpp
    src/computermodel.h
    src/machinedetailscontroller.cpp
    src/machinedetailscontroller.h
    src/migrationcontroller.cpp
    src/migrationcontroller.h
    src/solcontroller.cpp
    src/solcontroller.h
    src/idercontroller.cpp
    src/idercontroller.h
    src/kvmcontroller.cpp
    src/kvmcontroller.h
    src/kvmframebuffer.cpp
    src/kvmframebuffer.h
    src/kvmviewer.cpp
    src/kvmviewer.h
    src/certmodel.cpp
    src/certmodel.h
    src/updater.h
)

set_source_files_properties(
    qml/Theme/Colors.qml
    qml/Theme/Type.qml
    qml/Theme/Motion.qml
    qml/Theme/ThemePalette.qml
    qml/Theme/ActivityHeartbeat.qml
    qml/Paths.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_translations(qumesh
    TS_FILES
        translations/qumesh_en.ts
    QM_FILES_OUTPUT_VARIABLE qm_files
    LUPDATE_OPTIONS
        -no-obsolete
)

qt_add_qml_module(qumesh
    URI QuMesh
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ComputerListView.qml
        qml/ComputerStatusPane.qml
        qml/EditComputerDialog.qml
        qml/MigrationBanner.qml
        qml/TitleBar.qml
        qml/StatusBar.qml
        qml/StatusLed.qml
        qml/Section.qml
        qml/Terminal.qml
        qml/IderPanel.qml
        qml/KvmPanel.qml
        qml/SessionWindow.qml
        qml/SolPanel.qml
        qml/MachineDetailsWindow.qml
        qml/CertificatesWindow.qml
        qml/CertTrustDialog.qml
        qml/AccentButton.qml
        qml/AppMark.qml
        qml/AppWindow.qml
        qml/CertPinFlash.qml
        qml/FlatButton.qml
        qml/HttpsBootPrompt.qml
        qml/OptInPrompt.qml
        qml/Paths.qml
        qml/PlatformErasePrompt.qml
        qml/SecureErasePrompt.qml
        qml/Theme/Colors.qml
        qml/Theme/Type.qml
        qml/Theme/Motion.qml
        qml/Theme/ThemePalette.qml
        qml/Theme/ActivityHeartbeat.qml
        qml/HeartbeatBar.qml
    RESOURCES
        qml/fonts/IBMPlexSans-Regular.ttf
        qml/fonts/IBMPlexSans-Medium.ttf
        qml/fonts/JetBrainsMono-Regular.ttf
)

target_include_directories(qumesh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(qumesh PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Svg
    qumesh_migrate
    qumesh_redir
    qumesh_terminal
    qumesh_ider
    qumesh_kvm
    qumesh_certs
    qumesh_wsman
    qumesh_ssh
)

# --- Branding -------------------------------------------------------
#
# The master SVGs in qt/branding/svg/ use `currentColor` so they pick
# up the surrounding text colour in CSS — but Qt's SVG renderer
# resolves it to black. Bake two variants at configure time (slate
# for the light theme, near-white for the dark) and let `AppMark.qml`
# pick the right one based on the active palette.

set(_branding_dir "${CMAKE_SOURCE_DIR}/branding")
set(_baked_icons_dir "${CMAKE_CURRENT_BINARY_DIR}/icons")
file(MAKE_DIRECTORY "${_baked_icons_dir}")

foreach(_svg_name IN ITEMS qumesh-mark qumesh-mark-small icon-sun icon-moon)
    file(READ "${_branding_dir}/svg/${_svg_name}.svg" _svg_src)
    string(REPLACE "currentColor" "#1e293b" _svg_slate "${_svg_src}")
    string(REPLACE "currentColor" "#E6EDF3" _svg_light "${_svg_src}")
    file(WRITE "${_baked_icons_dir}/${_svg_name}-slate.svg" "${_svg_slate}")
    file(WRITE "${_baked_icons_dir}/${_svg_name}-light.svg" "${_svg_light}")
endforeach()

qt_add_resources(qumesh "qumesh_icons"
    PREFIX "/icons"
    BASE   "${_baked_icons_dir}"
    FILES
        "${_baked_icons_dir}/qumesh-mark-slate.svg"
        "${_baked_icons_dir}/qumesh-mark-light.svg"
        "${_baked_icons_dir}/qumesh-mark-small-slate.svg"
        "${_baked_icons_dir}/qumesh-mark-small-light.svg"
        "${_baked_icons_dir}/icon-sun-slate.svg"
        "${_baked_icons_dir}/icon-sun-light.svg"
        "${_baked_icons_dir}/icon-moon-slate.svg"
        "${_baked_icons_dir}/icon-moon-light.svg"
)

# Standalone tile asset (slate background + light Q) for the window
# icon — see main.cpp's QGuiApplication::setWindowIcon. Lives under
# branding/svg/ rather than the baked dir because it doesn't need a
# colour swap; the slate background and light-grey mark are baked in.
qt_add_resources(qumesh "qumesh_tile_icon"
    PREFIX "/icons"
    BASE   "${_branding_dir}/svg"
    FILES
        "${_branding_dir}/svg/qumesh-mark-tile.svg"
)

# Platform app icons (already pre-built under branding/build/).
if(APPLE)
    set(_macos_icns "${_branding_dir}/build/qumesh.icns")
    set_source_files_properties("${_macos_icns}"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(qumesh PRIVATE "${_macos_icns}")
endif()

# --- In-app auto-update ----------------------------------------------
#
# Two platform backends share the QML-facing `Updater` facade
# (src/updater.h): Sparkle on macOS and WinSparkle on Windows. Both
# binaries are pulled at configure time from their respective GitHub
# Releases (pinned URL + SHA256, no vendored binary in the repo). The
# bundle/install steps drop the framework / DLL next to the executable,
# and a checked-in public key in this directory (`sparkle_pub_ed_key`,
# `winsparkle_pub_ed_key`) is read in for the runtime signature check.
# Without a key set the build still produces a runnable artifact;
# Sparkle/WinSparkle simply refuse to install any update they can't
# verify.

option(QUMESH_AUTOUPDATE "Build with in-app auto-update (Sparkle/WinSparkle)" ON)

set(QUMESH_APPCAST_FEED_URL
    "https://github.com/benmcollins/QuMesh/releases/latest/download/appcast.xml"
    CACHE STRING "Sparkle/WinSparkle appcast.xml feed URL")

if(APPLE AND QUMESH_AUTOUPDATE)
    include(FetchContent)
    FetchContent_Declare(
        sparkle
        URL      https://github.com/sparkle-project/Sparkle/releases/download/2.6.4/Sparkle-2.6.4.tar.xz
        URL_HASH SHA256=50612a06038abc931f16011d7903b8326a362c1074dabccb718404ce8e585f0b
    )
    FetchContent_MakeAvailable(sparkle)

    set(_sparkle_framework "${sparkle_SOURCE_DIR}/Sparkle.framework")

    add_library(Sparkle::Sparkle SHARED IMPORTED)
    set_target_properties(Sparkle::Sparkle PROPERTIES
        FRAMEWORK TRUE
        IMPORTED_LOCATION             "${_sparkle_framework}/Sparkle"
        INTERFACE_INCLUDE_DIRECTORIES "${_sparkle_framework}/Headers"
    )

    target_sources(qumesh PRIVATE src/updater.mm)
    target_link_libraries(qumesh PRIVATE Sparkle::Sparkle "-framework AppKit")

    # Sparkle.framework lives next to the app inside Contents/Frameworks
    # at runtime; copy it in post-build so launch-from-build-tree also
    # works, and add the matching rpath.
    set_target_properties(qumesh PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    add_custom_command(TARGET qumesh POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:qumesh>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_sparkle_framework}"
            "$<TARGET_BUNDLE_DIR:qumesh>/Contents/Frameworks/Sparkle.framework"
        VERBATIM
    )

    # Custom Info.plist template so we can inject the Sparkle keys.
    # Read the EdDSA public key from a checked-in file; empty file is
    # fine for unsigned dev builds (Sparkle just refuses to install).
    set(_sparkle_pub_key_file "${CMAKE_CURRENT_SOURCE_DIR}/sparkle_pub_ed_key")
    if(EXISTS "${_sparkle_pub_key_file}")
        file(READ "${_sparkle_pub_key_file}" SPARKLE_PUBLIC_ED_KEY)
        string(STRIP "${SPARKLE_PUBLIC_ED_KEY}" SPARKLE_PUBLIC_ED_KEY)
    else()
        set(SPARKLE_PUBLIC_ED_KEY "")
    endif()
    set(SPARKLE_FEED_URL "${QUMESH_APPCAST_FEED_URL}")
    set_target_properties(qumesh PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
elseif(WIN32 AND QUMESH_AUTOUPDATE)
    include(FetchContent)
    FetchContent_Declare(
        winsparkle
        URL      https://github.com/vslavik/winsparkle/releases/download/v0.9.2/WinSparkle-0.9.2.zip
        URL_HASH SHA256=2a5facf5a22056edf7afc4d0a4bf05620b7aa7cc1619c3078b281e374cdb9996
    )
    FetchContent_MakeAvailable(winsparkle)

    # The zip lays out per-arch subdirs under WinSparkle-0.9.2/. We
    # build x64 (Qt's win64_msvc2022_64) so pick that variant.
    set(_winsparkle_root "${winsparkle_SOURCE_DIR}")
    set(_winsparkle_dll  "${_winsparkle_root}/x64/Release/WinSparkle.dll")
    set(_winsparkle_lib  "${_winsparkle_root}/x64/Release/WinSparkle.lib")

    add_library(WinSparkle::WinSparkle SHARED IMPORTED)
    set_target_properties(WinSparkle::WinSparkle PROPERTIES
        IMPORTED_LOCATION             "${_winsparkle_dll}"
        IMPORTED_IMPLIB               "${_winsparkle_lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${_winsparkle_root}/include"
    )

    target_sources(qumesh PRIVATE src/updater_win.cpp)
    target_link_libraries(qumesh PRIVATE WinSparkle::WinSparkle)

    # WinSparkle.dll must sit alongside QuMesh.exe at runtime. Copy
    # into the build dir post-build so launch-from-build-tree works,
    # and install it next to the exe so cpack picks it up.
    add_custom_command(TARGET qumesh POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_winsparkle_dll}"
            "$<TARGET_FILE_DIR:qumesh>/WinSparkle.dll"
        VERBATIM
    )
    install(FILES "${_winsparkle_dll}" DESTINATION .)

    # WinSparkle takes its appcast URL + EdDSA public key via C-API
    # calls at startup. Bake both into a generated header so the wiring
    # stays internal to the binary and we avoid MSVC/clang macro-quoting
    # differences that bite you with COMPILE_DEFINITIONS.
    #
    # Sparkle and WinSparkle both use Ed25519 with the same key format
    # (base64 single line, ~44 chars), so one keypair is reused across
    # macOS and Windows — there's only `sparkle_pub_ed_key`, no separate
    # winsparkle file.
    set(_appcast_pub_key_file "${CMAKE_CURRENT_SOURCE_DIR}/sparkle_pub_ed_key")
    if(EXISTS "${_appcast_pub_key_file}")
        file(READ "${_appcast_pub_key_file}" WINSPARKLE_PUBLIC_ED_KEY)
        string(STRIP "${WINSPARKLE_PUBLIC_ED_KEY}" WINSPARKLE_PUBLIC_ED_KEY)
    else()
        set(WINSPARKLE_PUBLIC_ED_KEY "")
    endif()
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/updater_win_config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/updater_win_config.h"
        @ONLY
    )
    target_include_directories(qumesh PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
else()
    # Stub implementation; declares Updater as available()==false so QML
    # can `import` it unconditionally and hide its UI.
    target_sources(qumesh PRIVATE src/updater.cpp)
endif()

if(WIN32)
    set(_win_ico "${_branding_dir}/build/qumesh.ico")
    file(TO_NATIVE_PATH "${_win_ico}" _win_ico_native)
    string(REPLACE "\\" "\\\\" _win_ico_native "${_win_ico_native}")
    set(_win_rc "${CMAKE_CURRENT_BINARY_DIR}/qumesh.rc")
    file(WRITE "${_win_rc}"
        "IDI_ICON1 ICON \"${_win_ico_native}\"\n")
    target_sources(qumesh PRIVATE "${_win_rc}")
endif()

set_target_properties(qumesh PROPERTIES
    OUTPUT_NAME "QuMesh"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "QuMesh"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.insynergy.QuMesh"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "qumesh.icns"
    WIN32_EXECUTABLE TRUE
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# --- Install / deploy -------------------------------------------------
#
# Install the app + the Qt runtime + OpenSSL into the install prefix.
# CPack at the top level picks this up to produce a .dmg / .exe.

if(APPLE)
    install(TARGETS qumesh BUNDLE DESTINATION .)
elseif(WIN32)
    install(TARGETS qumesh RUNTIME DESTINATION .)
else()
    install(TARGETS qumesh RUNTIME DESTINATION bin)
endif()

# Bundle OpenSSL's libcrypto next to the app. macdeployqt will follow
# it as a dependency on macOS once it lives inside the bundle;
# windeployqt does not pull in non-Qt DLLs so we install it manually.
if(WIN32)
    if(TARGET OpenSSL::Crypto)
        get_target_property(_ossl_loc OpenSSL::Crypto IMPORTED_LOCATION)
        if(_ossl_loc)
            install(FILES "${_ossl_loc}" DESTINATION .)
        endif()
        # OpenSSL 3 ships as libcrypto-3-x64.dll + a separate libssl.
        # Glob the directory so we pick up any sibling DLLs.
        get_filename_component(_ossl_dir "${_ossl_loc}" DIRECTORY)
        file(GLOB _ossl_dlls "${_ossl_dir}/libcrypto-*.dll")
        if(_ossl_dlls)
            install(FILES ${_ossl_dlls} DESTINATION .)
        endif()
    endif()
endif()

# Qt 6.5+ helper: produces a per-target deploy script that invokes
# macdeployqt / windeployqt with the right QML scan roots. The deploy
# script runs at install time, so it's already aware of our install
# prefix and bundle layout.
qt_generate_deploy_qml_app_script(
    TARGET qumesh
    OUTPUT_SCRIPT qumesh_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${qumesh_deploy_script})

# --- Code signing stubs ----------------------------------------------
#
# When CI provisions the relevant secrets, these blocks codesign the
# bundle before CPack picks it up. Until then they are no-ops; the
# unsigned bundle still produces a runnable .dmg / .exe (with the usual
# Gatekeeper / SmartScreen prompts on first launch).

if(APPLE AND DEFINED ENV{MAC_SIGNING_IDENTITY} AND NOT "$ENV{MAC_SIGNING_IDENTITY}" STREQUAL "")
    install(CODE "
        execute_process(
            COMMAND bash
                \"${CMAKE_CURRENT_SOURCE_DIR}/codesign-bundle.sh\"
                \"\${CMAKE_INSTALL_PREFIX}/QuMesh.app\"
                \"$ENV{MAC_SIGNING_IDENTITY}\"
                \"${CMAKE_CURRENT_SOURCE_DIR}/QuMesh.entitlements\"
            RESULT_VARIABLE _sign_rc
        )
        if(NOT _sign_rc EQUAL 0)
            message(FATAL_ERROR \"codesign-bundle.sh failed (\${_sign_rc})\")
        endif()
    ")
endif()

if(WIN32 AND DEFINED ENV{WIN_SIGN_THUMBPRINT} AND NOT "$ENV{WIN_SIGN_THUMBPRINT}" STREQUAL "")
    install(CODE "
        execute_process(COMMAND signtool sign /sha1 \"$ENV{WIN_SIGN_THUMBPRINT}\"
                                  /fd SHA256 /tr http://timestamp.digicert.com /td SHA256
                                  \"\${CMAKE_INSTALL_PREFIX}/QuMesh.exe\"
                          RESULT_VARIABLE _sign_rc)
        if(NOT _sign_rc EQUAL 0)
            message(FATAL_ERROR \"signtool sign failed (\${_sign_rc})\")
        endif()
    ")
endif()
