qt_add_library(meshcommander_wsman STATIC
    src/soap_envelope.cpp
    src/wsman_client.cpp
    src/operations.cpp
    include/wsman/soap_envelope.h
    include/wsman/wsman_client.h
    include/wsman/operations.h
)

target_include_directories(meshcommander_wsman
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_wsman
    PUBLIC
        Qt6::Core
        Qt6::Network
)

if(MSVC)
    target_compile_options(meshcommander_wsman PRIVATE /W4)
else()
    target_compile_options(meshcommander_wsman PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(meshcommander-wsman
    cli/main.cpp
)
target_link_libraries(meshcommander-wsman PRIVATE
    Qt6::Core
    meshcommander_wsman
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
