# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

cmake_minimum_required(VERSION 3.21)

project(QuMesh
    VERSION 0.1.0
    DESCRIPTION "QuMesh — native Qt6/QML console for Intel AMT (vPro) devices"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Svg Test LinguistTools)
find_package(Qt6 6.6 OPTIONAL_COMPONENTS HttpServer)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

# leveldb (read-only access to the legacy NW.js Chromium DOMStorage)
include(FetchContent)
set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LEVELDB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    leveldb
    GIT_REPOSITORY https://github.com/google/leveldb.git
    GIT_TAG       1.23
    GIT_SHALLOW   TRUE
)
FetchContent_MakeAvailable(leveldb)

include(CTest)

add_subdirectory(terminal)
add_subdirectory(certs)
add_subdirectory(app)
add_subdirectory(migrate)
add_subdirectory(wsman)
add_subdirectory(redir)
add_subdirectory(ider)
add_subdirectory(kvm)

# --- Packaging ---------------------------------------------------------
#
# `cpack` produces a .dmg on macOS (CPack DragNDrop) and an NSIS-based
# .exe installer on Windows. Both bundle Qt + plugins (via macdeployqt /
# windeployqt) plus OpenSSL.
#
# Cut a release: bump PROJECT_VERSION above, push a `vX.Y.Z` tag, and
# the GitHub Actions release workflow handles the rest.

set(CPACK_PACKAGE_NAME "QuMesh")
set(CPACK_PACKAGE_VENDOR "Insynergy")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/benmcollins/QuMesh")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Native Qt6 console for Intel AMT (vPro) devices")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "QuMesh")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_STRIP_FILES TRUE)

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "QuMesh-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "QuMesh-${PROJECT_VERSION}-macos")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "QuMesh")
    set(CPACK_NSIS_PACKAGE_NAME "QuMesh")
    set(CPACK_NSIS_HELP_LINK "https://github.com/benmcollins/QuMesh")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/benmcollins/QuMesh")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "QuMesh-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "QuMesh-${PROJECT_VERSION}-windows-x64")
endif()

include(CPack)
