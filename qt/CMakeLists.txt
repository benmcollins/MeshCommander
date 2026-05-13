cmake_minimum_required(VERSION 3.21)

project(MeshCommanderQt
    VERSION 0.1.0
    DESCRIPTION "Native Qt6/QML rewrite of MeshCommander"
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

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Quick QuickControls2 Network Test)
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

add_subdirectory(app)
add_subdirectory(migrate)
add_subdirectory(wsman)
add_subdirectory(redir)
