add_library(
  vulkan-renderer OBJECT
  VKPipeline.cpp
  VKPipeline.h
  VKPhysicalDevice.cpp
  VKPhysicalDevice.h
  VKSwapChain.cpp
  VKSwapChain.h
  VKConstants.h
  VKInstance.cpp
  VKInstance.h
  VKLogicalDevice.cpp
  VKLogicalDevice.h
  VKRenderPipeline.cpp
  VKRenderPipeline.h
  VKRenderPass.cpp
  VKRenderPass.h
  VKUniformBuffer.cpp
  VKUniformBuffer.h
  VKTexture.cpp
  VKTexture.h)
set_target_properties(vulkan-renderer PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
