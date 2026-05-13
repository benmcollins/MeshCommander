qt_add_executable(meshcommander
    src/main.cpp
)

qt_add_qml_module(meshcommander
    URI MeshCommander
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(meshcommander PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
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
