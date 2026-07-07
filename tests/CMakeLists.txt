add_executable(nova_timeline_tests timeline_tests.cpp)
target_link_libraries(nova_timeline_tests PRIVATE nova_timeline nova_core)
add_test(NAME nova_timeline_tests COMMAND nova_timeline_tests)

add_executable(nova_decoder_tests decoder_tests.cpp)
target_link_libraries(nova_decoder_tests PRIVATE nova_media nova_core)
add_test(NAME nova_decoder_tests COMMAND nova_decoder_tests)

add_executable(nova_project_tests project_tests.cpp)
target_link_libraries(nova_project_tests PRIVATE nova_project nova_timeline nova_core Qt6::Core)
add_test(NAME nova_project_tests COMMAND nova_project_tests)

if(WIN32)
    add_custom_command(TARGET nova_project_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE_DIR:nova_project_tests>"
        COMMENT "Copy Qt6Core.dll for nova_project_tests"
    )
endif()
