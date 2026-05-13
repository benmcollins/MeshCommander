function(meshcommander_app_test name)
    qt_add_executable(${name}
        ${name}.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/configstore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/configstore.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/computermodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/computermodel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/migrationcontroller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/migrationcontroller.h
    )
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Test
        meshcommander_migrate
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qt_add_executable(test_smoke test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE Qt6::Core Qt6::Test)
add_test(NAME smoke COMMAND test_smoke)

meshcommander_app_test(test_configstore)
meshcommander_app_test(test_computermodel)
meshcommander_app_test(test_migrationcontroller)
# test_migrationcontroller also seeds a real leveldb fixture.
target_link_libraries(test_migrationcontroller PRIVATE leveldb)
