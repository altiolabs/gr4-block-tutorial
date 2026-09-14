set(GrTutorialBlocks_HDRS
  include/gnuradio-4.0/tutorial/Gain.hpp)

add_library(gr-tutorial INTERFACE)
add_library(gnuradio4::gr-tutorial ALIAS gr-tutorial)
target_link_libraries(gr-tutorial INTERFACE gnuradio4::gnuradio-core)
target_include_directories(gr-tutorial INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(TARGETS gr-tutorial EXPORT gnuradio4TutorialTargets)
install(DIRECTORY include/gnuradio-4.0/tutorial DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gnuradio-4.0)

if(GrTutorialBlocks_HDRS)
  add_library(GrTutorialBlocksObject OBJECT)
  set_target_properties(GrTutorialBlocksObject PROPERTIES POSITION_INDEPENDENT_CODE ON)
  gr_generate_block_instantiations(GrTutorialBlocksObject
    HEADERS ${GrTutorialBlocks_HDRS} MODULE_NAME_BASE GrTutorialBlocks)
  target_link_libraries(GrTutorialBlocksObject PUBLIC gr-tutorial gnuradio4::gnuradio-blocklib-core)
  add_library(GrTutorialBlocksShared SHARED plugin.cpp $<TARGET_OBJECTS:GrTutorialBlocksObject>)
  add_library(gnuradio4::GrTutorialBlocksShared ALIAS GrTutorialBlocksShared)
  target_link_libraries(GrTutorialBlocksShared PUBLIC gnuradio4::gnuradio-core gnuradio4::gnuradio-blocklib-core)
  target_link_libraries(GrTutorialBlocksShared PRIVATE gnuradio4::gnuradio-plugin)
  target_include_directories(GrTutorialBlocksShared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  install(TARGETS GrTutorialBlocksShared EXPORT gnuradio4TutorialTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gnuradio-4/plugins)
  install(FILES ${CMAKE_BINARY_DIR}/include/gnuradio-4.0/GrTutorialBlocks.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gnuradio-4.0)
endif()
if(ENABLE_TESTING)
  add_subdirectory(test)
endif()
if(ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()
