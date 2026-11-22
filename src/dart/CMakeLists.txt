# ==============================================================================
# Bird Engine Dart Scripting & Automatic FFI Binding Generation
# ==============================================================================

# Curated script-facing C++ headers (Pass 1 Core + Pass 2 2D UI Framework)
set(BIRD_SCRIPT_HEADERS
    ${CMAKE_SOURCE_DIR}/src/util/Math.h
    ${CMAKE_SOURCE_DIR}/src/util/Input.h
    ${CMAKE_SOURCE_DIR}/src/util/Resource.h
    ${CMAKE_SOURCE_DIR}/src/util/ResourceManager.h
    ${CMAKE_SOURCE_DIR}/src/scene/Object.h
    ${CMAKE_SOURCE_DIR}/src/scene/WorldObject.h
    ${CMAKE_SOURCE_DIR}/src/scene/Scene.h
    ${CMAKE_SOURCE_DIR}/src/scene/Camera.h
    ${CMAKE_SOURCE_DIR}/src/scene/components/LightComponent.h
    ${CMAKE_SOURCE_DIR}/src/scene/components/Component.h
    ${CMAKE_SOURCE_DIR}/src/scene/components/Script.h
    ${CMAKE_SOURCE_DIR}/src/scene/components/ProcessScript.h
    ${CMAKE_SOURCE_DIR}/src/scene/components/MeshComponent.h
    ${CMAKE_SOURCE_DIR}/src/renders/generic/Material.h
    ${CMAKE_SOURCE_DIR}/src/physics/PhysicsLayers.h
    ${CMAKE_SOURCE_DIR}/src/physics/ContactEvent.h
    ${CMAKE_SOURCE_DIR}/src/physics/FixedStepClock.h
    ${CMAKE_SOURCE_DIR}/src/physics/CollisionShape.h
    ${CMAKE_SOURCE_DIR}/src/physics/Joint.h
    ${CMAKE_SOURCE_DIR}/src/physics/PhysicsWorld.h
    ${CMAKE_SOURCE_DIR}/src/physics/PhysicsServer.h
    ${CMAKE_SOURCE_DIR}/src/physics/RigidBodyComponent.h
    ${CMAKE_SOURCE_DIR}/src/physics/CharacterControllerComponent.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/ScreenObject.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UILayout.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIEvents.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIMenu.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UITheme.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIObject.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIManager.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UILabel.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIPanel.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIButton.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIStack.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UICheckBox.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UISlider.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIProgressBar.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UITextBox.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UITextArea.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIScrollView.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UITabView.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIWindow.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIDialog.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIDropdown.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIRadioGroup.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIToggle.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UINumberField.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIMenuBar.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIMenuPanel.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIListView.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UITreeView.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIExpander.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIColor.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIColorControls.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIColorPicker.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIRichInputs.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIVisualTransform.h
    ${CMAKE_SOURCE_DIR}/src/scene/2D/UIAnimator.h
    ${CMAKE_SOURCE_DIR}/src/animation/Easing.h
    ${CMAKE_SOURCE_DIR}/src/animation/Curve.h
    ${CMAKE_SOURCE_DIR}/src/animation/PlaybackClock.h
    ${CMAKE_SOURCE_DIR}/src/animation/AnimationSystem.h
    ${CMAKE_SOURCE_DIR}/src/animation/Curves.h
    ${CMAKE_SOURCE_DIR}/src/animation/WorldTweens.h
    ${CMAKE_SOURCE_DIR}/src/animation/skeletal/SkeletalAnimationComponent.h
    ${CMAKE_SOURCE_DIR}/src/audio/AudioTypes.h
    ${CMAKE_SOURCE_DIR}/src/audio/AudioSystem.h
    ${CMAKE_SOURCE_DIR}/src/audio/AudioSourceComponent.h
    ${CMAKE_SOURCE_DIR}/src/audio/AudioListenerComponent.h
    ${CMAKE_SOURCE_DIR}/src/Application.h
)

set(BIRD_DART_GEN_C_DIR "${CMAKE_BINARY_DIR}/generated/dart_c")
set(BIRD_DART_GEN_DART_DIR "${CMAKE_SOURCE_DIR}/dart/bird_engine/lib/src/generated")

set(BIRD_DART_C_WRAPPER_H "${BIRD_DART_GEN_C_DIR}/c_wrapper.h")
set(BIRD_DART_C_WRAPPER_CPP "${BIRD_DART_GEN_C_DIR}/c_wrapper.cpp")

add_custom_command(
    OUTPUT
        ${BIRD_DART_C_WRAPPER_H}
        ${BIRD_DART_C_WRAPPER_CPP}
        ${BIRD_DART_GEN_DART_DIR}/bindings.dart
        ${BIRD_DART_GEN_DART_DIR}/generated.dart
    COMMAND $<TARGET_FILE:DartApiGenerator>
        --out-c ${BIRD_DART_GEN_C_DIR}
        --out-dart ${BIRD_DART_GEN_DART_DIR}
        --lib libBird_core
        --
        ${BIRD_DART_GENERATOR_PARSE_FLAGS}
        -I${CMAKE_SOURCE_DIR}/src
        -I${INSTALLED_LIB_PATH}/include
        ${BIRD_SCRIPT_HEADERS}
    DEPENDS
        DartApiGenerator
        ${BIRD_SCRIPT_HEADERS}
    COMMENT "Generating extern \"C\" wrapper and Dart FFI bindings..."
    VERBATIM
)

# Automatically bootstrap Dart package dependencies (`dart pub get`) if never done before
find_program(BIRD_DART_EXE
    NAMES dart
    HINTS "${INSTALLED_LIB_PATH}/dart-sdk/bin"
)

set(_BIRD_DART_PKG_DEPS "")
if(BIRD_DART_EXE)
    set(_BIRD_ENGINE_PKG_CFG "${CMAKE_SOURCE_DIR}/dart/bird_engine/.dart_tool/package_config.json")
    add_custom_command(
        OUTPUT ${_BIRD_ENGINE_PKG_CFG}
        COMMAND ${BIRD_DART_EXE} pub get
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/dart/bird_engine
        DEPENDS ${CMAKE_SOURCE_DIR}/dart/bird_engine/pubspec.yaml
        COMMENT "Running dart pub get for dart/bird_engine..."
        VERBATIM
    )
    list(APPEND _BIRD_DART_PKG_DEPS ${_BIRD_ENGINE_PKG_CFG})

    if(EXISTS "${CMAKE_SOURCE_DIR}/test/pubspec.yaml")
        set(_BIRD_TEST_PKG_CFG "${CMAKE_SOURCE_DIR}/test/.dart_tool/package_config.json")
        add_custom_command(
            OUTPUT ${_BIRD_TEST_PKG_CFG}
            COMMAND ${BIRD_DART_EXE} pub get
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
            DEPENDS
                ${CMAKE_SOURCE_DIR}/test/pubspec.yaml
                ${CMAKE_SOURCE_DIR}/dart/bird_engine/pubspec.yaml
                ${_BIRD_ENGINE_PKG_CFG}
            COMMENT "Running dart pub get for test/..."
            VERBATIM
        )
        list(APPEND _BIRD_DART_PKG_DEPS ${_BIRD_TEST_PKG_CFG})
    endif()
else()
    message(WARNING
        "Dart CLI ('dart') not found in PATH or ${INSTALLED_LIB_PATH}/dart-sdk/bin. "
        "Skipping automatic 'dart pub get'; run 'dart pub get' manually in dart/bird_engine and test.")
endif()

add_custom_target(GenerateDartBindings
    DEPENDS
        ${BIRD_DART_C_WRAPPER_H}
        ${BIRD_DART_C_WRAPPER_CPP}
        ${BIRD_DART_GEN_DART_DIR}/bindings.dart
        ${BIRD_DART_GEN_DART_DIR}/generated.dart
        ${_BIRD_DART_PKG_DEPS}
)

# Locate dart_shared_library (libdart_dll)
set(_DART_DLL_ROOT "")
if(EXISTS "${INSTALLED_LIB_PATH}/dart_shared_library/include/dart_dll.h")
    set(_DART_DLL_ROOT "${INSTALLED_LIB_PATH}/dart_shared_library")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/libs/dart_shared_library/include/dart_dll.h")
    set(_DART_DLL_ROOT "${CMAKE_SOURCE_DIR}/libs/dart_shared_library")
endif()

if(NOT _DART_DLL_ROOT)
    message(FATAL_ERROR
        "BIRD_ENABLE_DART is ON, but dart_shared_library (dart_dll.h) was not found in "
        "${INSTALLED_LIB_PATH}/dart_shared_library or ${CMAKE_SOURCE_DIR}/libs/dart_shared_library.\n"
        "Install dependencies first:\n"
        "  cmake -S libs -B libs && cmake --build libs --target DART_SHARED_LIB\n"
        "(or disable Dart with -DBIRD_ENABLE_DART=OFF).")
endif()

if(WIN32)
    set(_DART_DLL_LIB "${_DART_DLL_ROOT}/bin/dart_dll.dll")
elseif(APPLE)
    set(_DART_DLL_LIB "${_DART_DLL_ROOT}/bin/libdart_dll.dylib")
else()
    set(_DART_DLL_LIB "${_DART_DLL_ROOT}/bin/libdart_dll.so")
endif()

add_library(dart_dll SHARED IMPORTED GLOBAL)
set_target_properties(dart_dll PROPERTIES
    IMPORTED_LOCATION "${_DART_DLL_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_DART_DLL_ROOT}/include"
)
if(WIN32)
    # MSVC links against the import library, not the DLL itself.
    set_target_properties(dart_dll PROPERTIES
        IMPORTED_IMPLIB "${_DART_DLL_ROOT}/bin/dart_dll.lib")
endif()

# Generated C wrapper + Embedded Dart VM runtime compiled directly into Bird_core
add_library(bird_dart OBJECT
    ${BIRD_DART_C_WRAPPER_CPP}
    DartRuntime.cpp
    DartRuntime.h
    DartScriptComponent.cpp
    DartScriptComponent.h
)
add_dependencies(bird_dart GenerateDartBindings)
target_compile_definitions(bird_dart PRIVATE BIRD_ENABLE_DART)
target_include_directories(bird_dart PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${BIRD_DART_GEN_C_DIR}
    ${INSTALLED_LIB_PATH}/include
    ${_DART_DLL_ROOT}/include
)
set_target_properties(bird_dart PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Expose path to libdart_dll so parent CMakeLists can copy it alongside Bird
set(BIRD_DART_DLL_PATH "${_DART_DLL_LIB}" PARENT_SCOPE)
set(BIRD_DART_GEN_C_DIR "${BIRD_DART_GEN_C_DIR}" PARENT_SCOPE)
