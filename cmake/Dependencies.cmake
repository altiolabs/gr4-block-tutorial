# Test-only dependency; GNU Radio itself always comes from the installed SDK.
option(GR_USE_FETCHCONTENT_DEPS "Allow downloading Boost.UT when it is not installed" ON)

add_library(tutorial-ut INTERFACE)
target_compile_definitions(tutorial-ut INTERFACE BOOST_UT_DISABLE_MODULE)

# Keep the explicit header path available for existing builds and offline use.
if(BOOST_UT_INCLUDE_DIR)
  if(NOT EXISTS "${BOOST_UT_INCLUDE_DIR}/boost/ut.hpp")
    message(FATAL_ERROR "BOOST_UT_INCLUDE_DIR must contain boost/ut.hpp.")
  endif()
  target_include_directories(tutorial-ut INTERFACE "${BOOST_UT_INCLUDE_DIR}")
  return()
endif()

find_package(ut CONFIG QUIET)
foreach(ut_target IN ITEMS Boost::ut boost::ut ut)
  if(TARGET ${ut_target})
    target_link_libraries(tutorial-ut INTERFACE ${ut_target})
    return()
  endif()
endforeach()

find_path(BOOST_UT_INCLUDE_DIR boost/ut.hpp)
if(BOOST_UT_INCLUDE_DIR)
  target_include_directories(tutorial-ut INTERFACE "${BOOST_UT_INCLUDE_DIR}")
  return()
endif()

if(NOT GR_USE_FETCHCONTENT_DEPS)
  message(FATAL_ERROR
    "Boost.UT was not found and downloads are disabled. Install Boost.UT, "
    "set BOOST_UT_INCLUDE_DIR, or enable GR_USE_FETCHCONTENT_DEPS.")
endif()

include(FetchContent)
set(BOOST_UT_DISABLE_MODULE ON)
set(BOOST_UT_BUILD_TESTS OFF)
set(BOOST_UT_BUILD_EXAMPLES OFF)
set(BOOST_UT_BUILD_BENCHMARKS OFF)
set(BOOST_UT_ENABLE_INSTALL OFF)
# Same revision as gnuradio4-blocks and gr4-incubator.
FetchContent_Declare(ut
  GIT_REPOSITORY https://github.com/boost-ext/ut.git
  GIT_TAG 53e17f25119598c6458d30351b260193096ba67e)
FetchContent_MakeAvailable(ut)
# Also suppress upstream install rules on CMake 3.27, which predates
# FetchContent_Declare's EXCLUDE_FROM_ALL option.
set_property(DIRECTORY "${ut_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
target_link_libraries(tutorial-ut INTERFACE Boost::ut)
