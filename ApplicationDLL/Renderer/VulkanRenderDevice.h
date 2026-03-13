#pragma once

#include "IRenderDevice.h"
#include "OpenGLRenderDevice.h"

#if defined(__has_include)
#if __has_include(<vulkan/vulkan.h>)
#define APPLICATIONDLL_HAS_VULKAN 1
#if !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#include <vulkan/vulkan.h>
#else
#define APPLICATIONDLL_HAS_VULKAN 0
#endif
#else
#define APPLICATIONDLL_HAS_VULKAN 0
#endif

#include <vector>

class VulkanRenderDevice final : public IRenderDevice
{
public:
    VulkanRenderDevice() = default;
    ~VulkanRenderDevice() override;

    RendererBackend Backend() const override { return RendererBackend::Vulkan; }

    bool Initialize(HWND hwnd, UINT width, UINT height) override;
    void Shutdown() override;
    bool Resize(UINT width, UINT height) override;
    void PreRender(const float clearColor[4]) override;
    void Render() override;
    bool SupportsEditorUi() const override;
    void SetImGuiDrawData(ImDrawData* drawData) override;
    void DrawQuadNdc(float centerX, float centerY, float width, float height) override;
    bool PrepareImGuiRenderContext() override;
    void CaptureEditorSceneTexture(UINT width, UINT height) override;
    bool HasEditorSceneTexture() const override;
    uintptr_t GetEditorSceneTextureHandle() const override;

#if APPLICATIONDLL_HAS_VULKAN
    bool IsUsingNativeVulkan() const { return useNativeVulkan_; }
    VkInstance GetVkInstance() const { return instance_; }
    VkPhysicalDevice GetVkPhysicalDevice() const { return physicalDevice_; }
    VkDevice GetVkDevice() const { return device_; }
    uint32_t GetVkGraphicsQueueFamilyIndex() const { return graphicsQueueFamilyIndex_; }
    VkQueue GetVkGraphicsQueue() const { return graphicsQueue_; }
    VkRenderPass GetVkRenderPass() const { return renderPass_; }
    VkDescriptorPool GetVkImGuiDescriptorPool() const { return imguiDescriptorPool_; }
    uint32_t GetVkSwapchainImageCount() const { return static_cast<uint32_t>(swapchainImages_.size()); }
#endif

private:
    struct QuadNdc
    {
        float centerX = 0.0f;
        float centerY = 0.0f;
        float width = 0.8f;
        float height = 1.4f;
    };

#if APPLICATIONDLL_HAS_VULKAN
    bool EnsureSceneCaptureResources(uint32_t width, uint32_t height, VkFormat format);
    void DestroySceneCaptureResources();
    bool RecordSceneCaptureCopy(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    uint32_t FindMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

    bool InitializeNativeVulkan(HWND hwnd, UINT width, UINT height);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain(UINT width, UINT height);
    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();
    bool CreateImGuiDescriptorPool();
    void CleanupSwapchain();
    void RecordCommandBuffer(uint32_t imageIndex);

    HWND hwnd_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex_ = UINT32_MAX;
    uint32_t presentQueueFamilyIndex_ = UINT32_MAX;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> imageViews_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
    VkDescriptorPool imguiDescriptorPool_ = VK_NULL_HANDLE;
    VkImage sceneCaptureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory sceneCaptureMemory_ = VK_NULL_HANDLE;
    VkImageView sceneCaptureView_ = VK_NULL_HANDLE;
    VkSampler sceneCaptureSampler_ = VK_NULL_HANDLE;
    VkDescriptorSet sceneCaptureDescriptorSet_ = VK_NULL_HANDLE;
    uint32_t sceneCaptureWidth_ = 0;
    uint32_t sceneCaptureHeight_ = 0;
    VkFormat sceneCaptureFormat_ = VK_FORMAT_UNDEFINED;
    bool sceneCaptureInitialized_ = false;
    bool sceneCaptureEnabled_ = false;
    bool useNativeVulkan_ = false;
#endif

    OpenGLRenderDevice fallback_;
    ImDrawData* pendingImGuiDrawData_ = nullptr;
    std::vector<QuadNdc> pendingQuads_;
    float pendingClearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};
