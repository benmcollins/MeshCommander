qt_add_library(meshcommander_migrate STATIC
    src/legacy_decrypt.cpp
    src/chromium_storage.cpp
    src/migrate.cpp
)

target_include_directories(meshcommander_migrate
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(meshcommander_migrate
    PUBLIC
        Qt6::Core
    PRIVATE
        OpenSSL::Crypto
        leveldb
)

# Suppress warnings inside vendored leveldb headers when consumers include
# <leveldb/db.h> indirectly through our public headers.
if(MSVC)
    target_compile_options(meshcommander_migrate PRIVATE /W4)
else()
    target_compile_options(meshcommander_migrate PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(meshcommander-migrate
    cli/main.cpp
)
target_link_libraries(meshcommander-migrate PRIVATE
    Qt6::Core
    meshcommander_migrate
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
