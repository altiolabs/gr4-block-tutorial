function(add_tutorial_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE gr-tutorial tutorial-ut gnuradio4::gnuradio-blocklib-core)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 30)
endfunction()
# Lesson 1 adds add_tutorial_test(qa_Gain).
