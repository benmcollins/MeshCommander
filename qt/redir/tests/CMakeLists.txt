function(meshcommander_redir_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Network
        meshcommander_redir
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

meshcommander_redir_test(test_redir_codec)
meshcommander_redir_test(test_redir_client)
