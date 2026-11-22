add_executable(Bird_unit_tests
  UIGeometryTest.cpp UITextTest.cpp UILayoutTest.cpp UIInputTest.cpp
  UITextEditTest.cpp UIWidgetTest.cpp UIDrawListTest.cpp UIPopupTest.cpp
  UIInputsV2Test.cpp UITextAreaTest.cpp UIMenuTest.cpp UIListTreeTest.cpp
  UIColorTest.cpp UIRichInputsTest.cpp PhysicsTest.cpp
  MaterialTest.cpp RenderQueueTest.cpp ShadowTest.cpp ResourceConventionTest.cpp
  GLCapabilitiesTest.cpp PlatformTest.cpp
  AnimationCoreTest.cpp UIAnimationTest.cpp SkeletalAnimationTest.cpp
  AdvancedAnimationTest.cpp)
target_include_directories(Bird_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Bird_unit_tests PRIVATE Bird_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(Bird_unit_tests)

# Bake tests link the BuildTools library only; no engine runtime or GL.
# The import test also plays the baked clip, so it compiles the runtime
# pieces it needs instead of linking the whole engine.
add_executable(Bird_bake_tests ShaderBakeTest.cpp SkeletalImportTest.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/PlaybackClock.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/AnimationParameters.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/BoneMask.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/PoseOps.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/PoseNode.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/BlendTree.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/AnimationStateMachine.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/TwoBoneIK.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/RootMotion.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/SkeletalDebugDraw.cpp
  ${CMAKE_SOURCE_DIR}/src/animation/skeletal/SkeletalAnimator.cpp)
target_link_libraries(Bird_bake_tests PRIVATE BirdBake GTest::gtest_main)
target_compile_definitions(Bird_bake_tests PRIVATE
  BIRD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(Bird_bake_tests)

if(BIRD_ENABLE_DART)
  add_executable(Bird_dart_tests DartIntegrationTest.cpp)
  target_include_directories(Bird_dart_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(Bird_dart_tests PRIVATE
    BIRD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  target_link_libraries(Bird_dart_tests PRIVATE Bird_core GTest::gtest_main)
  if(UNIX AND NOT APPLE)
    target_link_options(Bird_dart_tests PRIVATE -Wl,--export-dynamic)
  endif()
  set_target_properties(Bird_dart_tests PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
  )
  gtest_discover_tests(Bird_dart_tests)
endif()
