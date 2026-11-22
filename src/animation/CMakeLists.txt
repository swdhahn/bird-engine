# Engine-wide animation: curves, easing, playback, tweens, skeletal runtime.
add_library(
  animation OBJECT
  Interpolation.h
  Easing.h
  Easing.cpp
  Curve.h
  Curves.h
  Curves.cpp
  PlaybackClock.h
  PlaybackClock.cpp
  AnimationPlayback.h
  AnimationPlayback.cpp
  AnimationSystem.h
  AnimationSystem.cpp
  Tween.h
  WorldTweens.h
  WorldTweens.cpp
  skeletal/Skeleton.h
  skeletal/Skeleton.cpp
  skeletal/SkeletalClip.h
  skeletal/SkeletalClip.cpp
  skeletal/AnimationNotify.h
  skeletal/AnimationParameters.h
  skeletal/AnimationParameters.cpp
  skeletal/BoneMask.h
  skeletal/BoneMask.cpp
  skeletal/PoseOps.h
  skeletal/PoseOps.cpp
  skeletal/PoseNode.h
  skeletal/PoseNode.cpp
  skeletal/BlendTree.h
  skeletal/BlendTree.cpp
  skeletal/AnimationStateMachine.h
  skeletal/AnimationStateMachine.cpp
  skeletal/AnimationLayer.h
  skeletal/TwoBoneIK.h
  skeletal/TwoBoneIK.cpp
  skeletal/RootMotion.h
  skeletal/RootMotion.cpp
  skeletal/SkeletalDebugDraw.h
  skeletal/SkeletalDebugDraw.cpp
  skeletal/SkeletalAnimator.h
  skeletal/SkeletalAnimator.cpp
  skeletal/SkeletalAsset.h
  skeletal/SkeletalAsset.cpp
  skeletal/SkeletalAnimationComponent.h
  skeletal/SkeletalAnimationComponent.cpp)

target_link_libraries(animation PRIVATE glm)
set_target_properties(animation PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(MSVC)
  set_target_properties(animation PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
