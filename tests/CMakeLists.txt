add_executable(Bird_unit_tests
  UIGeometryTest.cpp UITextTest.cpp UILayoutTest.cpp UIInputTest.cpp
  UITextEditTest.cpp UIWidgetTest.cpp UIDrawListTest.cpp UIPopupTest.cpp
  UIInputsV2Test.cpp UITextAreaTest.cpp UIMenuTest.cpp UIListTreeTest.cpp
  UIColorTest.cpp UIRichInputsTest.cpp PhysicsTest.cpp)
target_include_directories(Bird_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Bird_unit_tests PRIVATE Bird_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(Bird_unit_tests)

# Bake tests link the BuildTools library only; no engine runtime or GL.
add_executable(Bird_bake_tests ShaderBakeTest.cpp)
target_link_libraries(Bird_bake_tests PRIVATE BirdBake GTest::gtest_main)
gtest_discover_tests(Bird_bake_tests)
