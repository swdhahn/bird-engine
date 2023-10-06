//
// Created by Seth Hahn on 11/1/22.
//
#ifdef _VULKAN_DEFINED
#ifndef BIRD_APPLICATION_H
#define BIRD_APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <optional>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#define STB_IMAGE_IMPLEMENTATION

namespace bird {

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                       VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);
*/
    extern const std::vector<const char*> validationLayers, deviceExtensions;

    extern const bool enableValidationLayers;

    extern const int MAX_FRAMES_IN_FLIGHT;
    extern const std::vector<Vertex> vertices;
    extern const std::vector<uint16_t> indices;

    class Application {
    public:
        void run();

    private:
        void initWindow();
        void initVulkan();
        void mainLoop();
        void cleanup();

        void createInstance();
        bool checkValidationSupport();
        void setupDebugMessenger();
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        void createLogicalDevice();
        void createSurface();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        void createSwapChain();
        void recreateSwapChain();
        void cleanupSwapChain();
        void createImageViews();
        void createRenderPass();
        void createGraphicsPipeline();
        VkShaderModule createShaderModule(const std::vector<char>& code);
        void createFramebuffers();
        void createVertexBuffer();
        void createIndexBuffer();
        void createCommandPool();
        void createCommandBuffers();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void createSyncObjects();
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void createDescriptorSetLayout();
        void createDescriptorPool();
        void createDescriptorSets();
        void createUniformBuffers();
        void updateUniformBuffer(uint32_t currentImage);
        void createTextureImage();
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void createTextureImageView();
        void createTextureSampler();
        void createDepthResources();
        void generateMipmaps(VkImage image, VkFormat  format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        void createColorResources();

        void drawFrame();

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        VkSurfaceFormatKHR chooseSwapChainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkFormat findDepthFormat();
        bool hasStencilComponent(VkFormat format);
        VkSampleCountFlagBits getMaxUsableSampleCount();

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData);
        static void resizeWindowGlfwCallback(GLFWwindow* window, int width, int height);
        static std::vector<char> readFile(const std::string& path);

        GLFWwindow* m_pWindow = nullptr;
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_device;
        VkQueue m_graphicsQueue;
        VkQueue m_presentQueue;
        VkSurfaceKHR m_surface;
        VkSwapchainKHR m_swapChain;
        std::vector<VkImage> m_swapChainImages;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;
        std::vector<VkImageView> m_swapChainImageViews;
        VkRenderPass m_renderPass;
        VkDescriptorSetLayout m_descriptorSetLayout;
        VkDescriptorPool m_descriptorPool;
        std::vector<VkDescriptorSet> m_descriptorSets;
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_graphicsPipeline;
        std::vector<VkFramebuffer> m_swapChainFramebuffers;
        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
        VkBuffer m_vertexBuffer;
        VkDeviceMemory m_vertexBufferMemory;
        VkBuffer m_indexBuffer;
        VkDeviceMemory m_indexBufferMemory;
        std::vector<VkBuffer> m_uniformBuffers;
        std::vector<VkDeviceMemory> m_uniformBuffersMemory;
        std::vector<void*> m_uniformBuffersMapped;
        uint32_t m_mipLevels;
        VkImage m_textureImage;
        VkDeviceMemory m_textureImageMemory;
        VkImageView m_textureImageView;
        VkSampler m_textureSampler;
        VkImage m_depthImage;
        VkDeviceMemory m_depthImageMemory;
        VkImageView m_depthImageView;
        VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkImage m_colorImage;
        VkDeviceMemory m_colorImageMemory;
        VkImageView m_colorImageView;

        uint32_t m_currentFrame = 0;
        bool m_framebufferResized = false;

        VkDebugUtilsMessengerEXT m_debugMessenger;

    };

} // bird

#endif //BIRD_APPLICATION_H
#endif