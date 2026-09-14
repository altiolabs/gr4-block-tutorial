add_executable(tutorial_registry tutorial_registry.cpp)
target_link_libraries(tutorial_registry PRIVATE gnuradio4::GrTutorialBlocksShared)
add_executable(tutorial_plugin_check tutorial_plugin_check.cpp)
target_link_libraries(tutorial_plugin_check PRIVATE gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
if(ENABLE_TESTING)
  add_test(NAME qa_TutorialPluginDiscovery COMMAND tutorial_plugin_check)
  set_tests_properties(qa_TutorialPluginDiscovery PROPERTIES TIMEOUT 30
    ENVIRONMENT_MODIFICATION "GNURADIO4_PLUGIN_DIRECTORIES=set:$<TARGET_FILE_DIR:GrTutorialBlocksShared>")
endif()
