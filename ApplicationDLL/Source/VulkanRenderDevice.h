#pragma once

#include "IRenderDevice.h"
#include "OpenGLRenderDevice.h"

#if defined(__has_include)
#if __has_include(<vulkan/vulkan.h>)
#define APPLICATIONDLL_HAS_VULKAN 1
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

private:
#if APPLICATIONDLL_HAS_VULKAN
    bool InitializeNativeVulkan(HWND hwnd, UINT width, UINT height);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain(UINT width, UINT height);
    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();
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
    bool useNativeVulkan_ = false;
#endif

    OpenGLRenderDevice fallback_;
    float pendingClearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};
