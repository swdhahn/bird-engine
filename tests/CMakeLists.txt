add_executable(Bird_unit_tests UIGeometryTest.cpp UIHitTest.cpp UITextTest.cpp)
target_include_directories(Bird_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Bird_unit_tests PRIVATE Bird_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(Bird_unit_tests)
