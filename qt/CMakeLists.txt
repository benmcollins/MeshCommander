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

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

include(CTest)

add_subdirectory(app)
