# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

qt_add_library(qumesh_kvm STATIC
    include/kvm/kvm_codec.h
    include/kvm/kvm_session.h
    src/kvm_codec.cpp
    src/kvm_session.cpp
)

target_include_directories(qumesh_kvm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Try the system zlib first (always present on macOS / Linux), fall
# back to building from source via FetchContent on Windows where the
# default GitHub-hosted runner image doesn't ship a discoverable zlib.
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    include(FetchContent)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG       v1.3.1
        GIT_SHALLOW   TRUE
        EXCLUDE_FROM_ALL
    )
    # EXCLUDE_FROM_ALL above also suppresses zlib's own install()
    # rules. Without this, CPack on Windows refuses to package the
    # bundle because zlib's CMakeLists installs zlib.lib to an
    # absolute path under "C:/Program Files (x86)/QuMesh/lib".
    FetchContent_MakeAvailable(zlib)
    # zlib's CMake exports targets `zlib` (shared) and `zlibstatic`.
    # Alias one of them as ZLIB::ZLIB so the link statement below
    # works regardless of which target name madler/zlib produced.
    if(NOT TARGET ZLIB::ZLIB)
        if(TARGET zlibstatic)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        else()
            add_library(ZLIB::ZLIB ALIAS zlib)
        endif()
    endif()
endif()

target_link_libraries(qumesh_kvm PUBLIC
    Qt6::Core
    Qt6::Gui
    qumesh_redir
    ZLIB::ZLIB
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
