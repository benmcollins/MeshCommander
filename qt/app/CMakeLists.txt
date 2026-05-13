qt_add_executable(meshcommander
    src/main.cpp
    src/configstore.cpp
    src/configstore.h
    src/computermodel.cpp
    src/computermodel.h
    src/migrationcontroller.cpp
    src/migrationcontroller.h
)

set_source_files_properties(
    qml/Theme/Colors.qml
    qml/Theme/Type.qml
    qml/Theme/Motion.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(meshcommander
    URI MeshCommander
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ComputerListView.qml
        qml/ComputerEditPane.qml
        qml/MigrationBanner.qml
        qml/TitleBar.qml
        qml/StatusBar.qml
        qml/StatusLed.qml
        qml/Section.qml
        qml/Theme/Colors.qml
        qml/Theme/Type.qml
        qml/Theme/Motion.qml
    RESOURCES
        qml/fonts/IBMPlexSans-Regular.ttf
        qml/fonts/IBMPlexSans-Medium.ttf
        qml/fonts/JetBrainsMono-Regular.ttf
)

target_include_directories(meshcommander PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(meshcommander PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    meshcommander_migrate
)

set_target_properties(meshcommander PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "MeshCommander"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.insynergy.MeshCommander"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
