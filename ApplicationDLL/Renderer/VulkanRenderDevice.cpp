#include "pch.h"
#include "VulkanRenderDevice.h"
#include "ThirdParty/imgui/imgui.h"
#if APPLICATIONDLL_HAS_VULKAN
#include "ThirdParty/imgui/backends/imgui_impl_vulkan.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#if APPLICATIONDLL_HAS_VULKAN
#pragma comment(lib, "vulkan-1.lib")

namespace
{
    bool CheckVk(VkResult result)
    {
        return result == VK_SUCCESS;
    }

    bool IsImGuiVulkanBackendReady()
    {
        if (ImGui::GetCurrentContext() == nullptr)
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (io.BackendRendererUserData == nullptr || io.BackendRendererName == nullptr)
        {
            return false;
        }

        return std::strstr(io.BackendRendererName, "imgui_impl_vulkan") != nullptr;
    }
}
#endif

VulkanRenderDevice::~VulkanRenderDevice()
{
    Shutdown();
}

bool VulkanRenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    fallback_.SetPresentBackendLabel("Vulkan(OpenGLFallback)");
#if APPLICATIONDLL_HAS_VULKAN
    if (InitializeNativeVulkan(hwnd, width, height))
    {
        useNativeVulkan_ = true;
        LOG_DEBUG("VulkanRenderDevice: native Vulkan initialized");
        return true;
    }

    LOG_DEBUG("VulkanRenderDevice: native Vulkan initialize failed. Using OpenGL fallback present path");
    useNativeVulkan_ = false;
#endif

    return fallback_.Initialize(hwnd, width, height);
}

void VulkanRenderDevice::Shutdown()
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        if (device_ != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device_);
            pendingImGuiDrawData_ = nullptr;
            pendingQuads_.clear();

            DestroySceneCaptureResources();

            if (inFlightFence_ != VK_NULL_HANDLE)
            {
                vkDestroyFence(device_, inFlightFence_, nullptr);
                inFlightFence_ = VK_NULL_HANDLE;
            }
            if (imageAvailableSemaphore_ != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
                imageAvailableSemaphore_ = VK_NULL_HANDLE;
            }
            if (renderFinishedSemaphore_ != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
                renderFinishedSemaphore_ = VK_NULL_HANDLE;
            }
            if (imguiDescriptorPool_ != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(device_, imguiDescriptorPool_, nullptr);
                imguiDescriptorPool_ = VK_NULL_HANDLE;
            }

            if (commandPool_ != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device_, commandPool_, nullptr);
                commandPool_ = VK_NULL_HANDLE;
                commandBuffers_.clear();
            }
        }
        CleanupSwapchain();

        if (renderPass_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_, renderPass_, nullptr);
            renderPass_ = VK_NULL_HANDLE;
        }

        if (device_ != VK_NULL_HANDLE)
        {
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
        }

        if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }
        if (instance_ != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }

        useNativeVulkan_ = false;
        physicalDevice_ = VK_NULL_HANDLE;
        graphicsQueueFamilyIndex_ = UINT32_MAX;
        presentQueueFamilyIndex_ = UINT32_MAX;
    }
#endif

    fallback_.Shutdown();
}

bool VulkanRenderDevice::Resize(UINT width, UINT height)
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        if (device_ == VK_NULL_HANDLE || width == 0 || height == 0)
        {
            return false;
        }

        vkDeviceWaitIdle(device_);
        CleanupSwapchain();

        if (!CreateSwapchain(width, height) ||
            !CreateImageViews() ||
            !CreateRenderPass() ||
            !CreateFramebuffers() ||
            !CreateCommandPoolAndBuffers())
        {
            return false;
        }
        if (sceneCaptureEnabled_ && !EnsureSceneCaptureResources(swapchainExtent_.width, swapchainExtent_.height, swapchainFormat_))
        {
            sceneCaptureEnabled_ = false;
        }
        return true;
    }
#endif

    return fallback_.Resize(width, height);
}

void VulkanRenderDevice::PreRender(const float clearColor[4])
{
    if (clearColor != nullptr)
    {
        std::memcpy(pendingClearColor_, clearColor, sizeof(pendingClearColor_));
    }

#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        pendingImGuiDrawData_ = nullptr;
        pendingQuads_.clear();
        return;
    }
#endif

    fallback_.PreRender(clearColor);
}

void VulkanRenderDevice::Render()
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE)
        {
            return;
        }

        vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, 1, &inFlightFence_);

        uint32_t imageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(
            device_,
            swapchain_,
            UINT64_MAX,
            imageAvailableSemaphore_,
            VK_NULL_HANDLE,
            &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Resize(swapchainExtent_.width, swapchainExtent_.height);
            return;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            return;
        }

        RecordCommandBuffer(imageIndex);

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore_;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore_;

        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS)
        {
            return;
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore_;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            Resize(swapchainExtent_.width, swapchainExtent_.height);
        }
        static uint32_t presentCounter = 0;
        ++presentCounter;
        if ((presentCounter % 240) == 0)
        {
            LOG_DEBUG("Active present state: backend=Vulkan(native) extent=(%u,%u) imageIndex=%u",
                swapchainExtent_.width,
                swapchainExtent_.height,
                imageIndex);
        }
        return;
    }
#endif

    fallback_.Render();
}

void VulkanRenderDevice::DrawQuadNdc(float centerX, float centerY, float width, float height)
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        QuadNdc quad = {};
        quad.centerX = centerX;
        quad.centerY = centerY;
        quad.width = (std::max)(width, 0.01f);
        quad.height = (std::max)(height, 0.01f);
        pendingQuads_.push_back(quad);
        return;
    }
#endif
    fallback_.DrawQuadNdc(centerX, centerY, width, height);
}

void VulkanRenderDevice::SetImGuiDrawData(ImDrawData* drawData)
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        pendingImGuiDrawData_ = drawData;
        return;
    }
#endif
    (void)drawData;
}

bool VulkanRenderDevice::SupportsEditorUi() const
{
    return true;
}

bool VulkanRenderDevice::PrepareImGuiRenderContext()
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        return true;
    }
#endif
    return fallback_.PrepareImGuiRenderContext();
}

void VulkanRenderDevice::CaptureEditorSceneTexture(UINT width, UINT height)
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        IM_UNUSED(width);
        IM_UNUSED(height);
        return;
    }
#endif
    fallback_.CaptureEditorSceneTexture(width, height);
}

bool VulkanRenderDevice::HasEditorSceneTexture() const
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        return sceneCaptureEnabled_ && sceneCaptureDescriptorSet_ != VK_NULL_HANDLE && sceneCaptureInitialized_;
    }
#endif
    return fallback_.HasEditorSceneTexture();
}

uintptr_t VulkanRenderDevice::GetEditorSceneTextureHandle() const
{
#if APPLICATIONDLL_HAS_VULKAN
    if (useNativeVulkan_)
    {
        return (uintptr_t)sceneCaptureDescriptorSet_;
    }
#endif
    return fallback_.GetEditorSceneTextureHandle();
}

#if APPLICATIONDLL_HAS_VULKAN
bool VulkanRenderDevice::InitializeNativeVulkan(HWND hwnd, UINT width, UINT height)
{
    hwnd_ = hwnd;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "DirectX12_Samples";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Samples";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const std::array<const char*, 2> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    if (!CheckVk(vkCreateInstance(&createInfo, nullptr, &instance_)))
    {
        return false;
    }

    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    surfaceInfo.hwnd = hwnd_;
    if (!CheckVk(vkCreateWin32SurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_)))
    {
        return false;
    }

    if (!PickPhysicalDevice() || !CreateLogicalDevice())
    {
        return false;
    }

    if (!CreateSwapchain(width, height) ||
        !CreateImageViews() ||
        !CreateRenderPass() ||
        !CreateFramebuffers() ||
        !CreateCommandPoolAndBuffers() ||
        !CreateSyncObjects() ||
        !CreateImGuiDescriptorPool())
    {
        return false;
    }

    if (sceneCaptureEnabled_ && !EnsureSceneCaptureResources(swapchainExtent_.width, swapchainExtent_.height, swapchainFormat_))
    {
        sceneCaptureEnabled_ = false;
    }

    return true;
}

bool VulkanRenderDevice::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (VkPhysicalDevice candidate : devices)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsIdx = UINT32_MAX;
        uint32_t presentIdx = UINT32_MAX;

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                graphicsIdx = i;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &presentSupport);
            if (presentSupport)
            {
                presentIdx = i;
            }
        }

        if (graphicsIdx != UINT32_MAX && presentIdx != UINT32_MAX)
        {
            const char* requiredDeviceExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            uint32_t extCount = 0;
            vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> exts(extCount);
            vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, exts.data());

            bool hasSwapchainExt = false;
            for (const auto& ext : exts)
            {
                if (std::strcmp(ext.extensionName, requiredDeviceExt) == 0)
                {
                    hasSwapchainExt = true;
                    break;
                }
            }

            if (hasSwapchainExt)
            {
                physicalDevice_ = candidate;
                graphicsQueueFamilyIndex_ = graphicsIdx;
                presentQueueFamilyIndex_ = presentIdx;
                return true;
            }
        }
    }

    return false;
}

bool VulkanRenderDevice::CreateLogicalDevice()
{
    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    VkDeviceQueueCreateInfo graphicsQueueInfo = {};
    graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueueInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;
    graphicsQueueInfo.queueCount = 1;
    graphicsQueueInfo.pQueuePriorities = &queuePriority;
    queueInfos.push_back(graphicsQueueInfo);

    if (presentQueueFamilyIndex_ != graphicsQueueFamilyIndex_)
    {
        VkDeviceQueueCreateInfo presentQueueInfo = graphicsQueueInfo;
        presentQueueInfo.queueFamilyIndex = presentQueueFamilyIndex_;
        queueInfos.push_back(presentQueueInfo);
    }

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (!CheckVk(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_)))
    {
        return false;
    }

    vkGetDeviceQueue(device_, graphicsQueueFamilyIndex_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamilyIndex_, 0, &presentQueue_);
    return true;
}

bool VulkanRenderDevice::CreateSwapchain(UINT width, UINT height)
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    if (!CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities)))
    {
        return false;
    }

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    if (formatCount == 0)
    {
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes((std::max)(presentModeCount, 1u));
    if (presentModeCount > 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());
    }

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceFormat = format;
            break;
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = mode;
            break;
        }
    }

    VkExtent2D extent = {};
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent.width = (std::max)(capabilities.minImageExtent.width, (std::min)(capabilities.maxImageExtent.width, width));
        extent.height = (std::max)(capabilities.minImageExtent.height, (std::min)(capabilities.maxImageExtent.height, height));
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sceneCaptureEnabled_ = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    if (sceneCaptureEnabled_)
    {
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex_, presentQueueFamilyIndex_ };
    if (graphicsQueueFamilyIndex_ != presentQueueFamilyIndex_)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (!CheckVk(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_)))
    {
        return false;
    }

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount, nullptr);
    swapchainImages_.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount, swapchainImages_.data());
    return true;
}

bool VulkanRenderDevice::CreateImageViews()
{
    imageViews_.resize(swapchainImages_.size());

    for (size_t i = 0; i < swapchainImages_.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (!CheckVk(vkCreateImageView(device_, &viewInfo, nullptr, &imageViews_[i])))
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderDevice::CreateRenderPass()
{
    if (renderPass_ != VK_NULL_HANDLE)
    {
        return true;
    }

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    return CheckVk(vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_));
}

bool VulkanRenderDevice::CreateFramebuffers()
{
    framebuffers_.resize(imageViews_.size());

    for (size_t i = 0; i < imageViews_.size(); ++i)
    {
        VkImageView attachments[] = { imageViews_[i] };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;

        if (!CheckVk(vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffers_[i])))
        {
            return false;
        }
    }
    return true;
}

bool VulkanRenderDevice::CreateCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (!CheckVk(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_)))
    {
        return false;
    }

    commandBuffers_.resize(framebuffers_.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    return CheckVk(vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()));
}

bool VulkanRenderDevice::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (!CheckVk(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_)))
    {
        return false;
    }
    if (!CheckVk(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_)))
    {
        return false;
    }
    if (!CheckVk(vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_)))
    {
        return false;
    }
    return true;
}

bool VulkanRenderDevice::CreateImGuiDescriptorPool()
{
    if (imguiDescriptorPool_ != VK_NULL_HANDLE)
    {
        return true;
    }

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 128 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 128 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 128 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 128 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 128 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 128 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 128 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 128u * static_cast<uint32_t>(_countof(poolSizes));
    poolInfo.poolSizeCount = static_cast<uint32_t>(_countof(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    return CheckVk(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imguiDescriptorPool_));
}

void VulkanRenderDevice::CleanupSwapchain()
{
    if (device_ == VK_NULL_HANDLE)
    {
        return;
    }

    for (VkFramebuffer framebuffer : framebuffers_)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();

    for (VkImageView view : imageViews_)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device_, view, nullptr);
        }
    }
    imageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();

    if (commandPool_ != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
        commandBuffers_.clear();
    }
}

uint32_t VulkanRenderDevice::FindMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const bool typeMatch = (typeBits & (1u << i)) != 0;
        const bool propertyMatch = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeMatch && propertyMatch)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

void VulkanRenderDevice::DestroySceneCaptureResources()
{
    if (device_ == VK_NULL_HANDLE)
    {
        return;
    }

    if (sceneCaptureDescriptorSet_ != VK_NULL_HANDLE)
    {
        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGui_ImplVulkan_RemoveTexture(sceneCaptureDescriptorSet_);
        }
        sceneCaptureDescriptorSet_ = VK_NULL_HANDLE;
    }

    if (sceneCaptureSampler_ != VK_NULL_HANDLE)
    {
        vkDestroySampler(device_, sceneCaptureSampler_, nullptr);
        sceneCaptureSampler_ = VK_NULL_HANDLE;
    }
    if (sceneCaptureView_ != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device_, sceneCaptureView_, nullptr);
        sceneCaptureView_ = VK_NULL_HANDLE;
    }
    if (sceneCaptureImage_ != VK_NULL_HANDLE)
    {
        vkDestroyImage(device_, sceneCaptureImage_, nullptr);
        sceneCaptureImage_ = VK_NULL_HANDLE;
    }
    if (sceneCaptureMemory_ != VK_NULL_HANDLE)
    {
        vkFreeMemory(device_, sceneCaptureMemory_, nullptr);
        sceneCaptureMemory_ = VK_NULL_HANDLE;
    }
    sceneCaptureWidth_ = 0;
    sceneCaptureHeight_ = 0;
    sceneCaptureFormat_ = VK_FORMAT_UNDEFINED;
    sceneCaptureInitialized_ = false;
}

bool VulkanRenderDevice::EnsureSceneCaptureResources(uint32_t width, uint32_t height, VkFormat format)
{
    if (!sceneCaptureEnabled_ || width == 0 || height == 0 || device_ == VK_NULL_HANDLE)
    {
        return false;
    }

    const bool recreate =
        sceneCaptureImage_ == VK_NULL_HANDLE ||
        sceneCaptureWidth_ != width ||
        sceneCaptureHeight_ != height ||
        sceneCaptureFormat_ != format;

    if (recreate)
    {
        DestroySceneCaptureResources();

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = { width, height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!CheckVk(vkCreateImage(device_, &imageInfo, nullptr, &sceneCaptureImage_)))
        {
            return false;
        }

        VkMemoryRequirements memReq = {};
        vkGetImageMemoryRequirements(device_, sceneCaptureImage_, &memReq);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == UINT32_MAX ||
            !CheckVk(vkAllocateMemory(device_, &allocInfo, nullptr, &sceneCaptureMemory_)))
        {
            DestroySceneCaptureResources();
            return false;
        }

        if (!CheckVk(vkBindImageMemory(device_, sceneCaptureImage_, sceneCaptureMemory_, 0)))
        {
            DestroySceneCaptureResources();
            return false;
        }

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = sceneCaptureImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (!CheckVk(vkCreateImageView(device_, &viewInfo, nullptr, &sceneCaptureView_)))
        {
            DestroySceneCaptureResources();
            return false;
        }

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        if (!CheckVk(vkCreateSampler(device_, &samplerInfo, nullptr, &sceneCaptureSampler_)))
        {
            DestroySceneCaptureResources();
            return false;
        }

        sceneCaptureWidth_ = width;
        sceneCaptureHeight_ = height;
        sceneCaptureFormat_ = format;
        sceneCaptureInitialized_ = false;
    }

    if (sceneCaptureDescriptorSet_ == VK_NULL_HANDLE && IsImGuiVulkanBackendReady())
    {
        sceneCaptureDescriptorSet_ = ImGui_ImplVulkan_AddTexture(
            sceneCaptureSampler_,
            sceneCaptureView_,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (sceneCaptureDescriptorSet_ == VK_NULL_HANDLE)
        {
            return false;
        }
    }

    return sceneCaptureImage_ != VK_NULL_HANDLE;
}

bool VulkanRenderDevice::RecordSceneCaptureCopy(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    if (!sceneCaptureEnabled_ || sceneCaptureImage_ == VK_NULL_HANDLE || imageIndex >= swapchainImages_.size())
    {
        return false;
    }

    VkImageMemoryBarrier barriersBefore[2] = {};
    barriersBefore[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriersBefore[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriersBefore[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriersBefore[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriersBefore[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriersBefore[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersBefore[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersBefore[0].image = swapchainImages_[imageIndex];
    barriersBefore[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriersBefore[0].subresourceRange.baseMipLevel = 0;
    barriersBefore[0].subresourceRange.levelCount = 1;
    barriersBefore[0].subresourceRange.baseArrayLayer = 0;
    barriersBefore[0].subresourceRange.layerCount = 1;

    barriersBefore[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriersBefore[1].srcAccessMask = sceneCaptureInitialized_ ? VK_ACCESS_SHADER_READ_BIT : 0;
    barriersBefore[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriersBefore[1].oldLayout = sceneCaptureInitialized_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    barriersBefore[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriersBefore[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersBefore[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersBefore[1].image = sceneCaptureImage_;
    barriersBefore[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriersBefore[1].subresourceRange.baseMipLevel = 0;
    barriersBefore[1].subresourceRange.levelCount = 1;
    barriersBefore[1].subresourceRange.baseArrayLayer = 0;
    barriersBefore[1].subresourceRange.layerCount = 1;

    const VkPipelineStageFlags srcStageBefore =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        (sceneCaptureInitialized_ ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageBefore,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        2, barriersBefore);

    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent.width = swapchainExtent_.width;
    copyRegion.extent.height = swapchainExtent_.height;
    copyRegion.extent.depth = 1;

    vkCmdCopyImage(
        commandBuffer,
        swapchainImages_[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        sceneCaptureImage_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion);

    VkImageMemoryBarrier barriersAfter[2] = {};
    barriersAfter[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriersAfter[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriersAfter[0].dstAccessMask = 0;
    barriersAfter[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriersAfter[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriersAfter[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersAfter[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersAfter[0].image = swapchainImages_[imageIndex];
    barriersAfter[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriersAfter[0].subresourceRange.baseMipLevel = 0;
    barriersAfter[0].subresourceRange.levelCount = 1;
    barriersAfter[0].subresourceRange.baseArrayLayer = 0;
    barriersAfter[0].subresourceRange.layerCount = 1;

    barriersAfter[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriersAfter[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriersAfter[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriersAfter[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriersAfter[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriersAfter[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersAfter[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriersAfter[1].image = sceneCaptureImage_;
    barriersAfter[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriersAfter[1].subresourceRange.baseMipLevel = 0;
    barriersAfter[1].subresourceRange.levelCount = 1;
    barriersAfter[1].subresourceRange.baseArrayLayer = 0;
    barriersAfter[1].subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        2, barriersAfter);
    sceneCaptureInitialized_ = true;
    return true;
}

void VulkanRenderDevice::RecordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkResetCommandBuffer(commandBuffer, 0);
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier toTransferDstBarrier = {};
    toTransferDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransferDstBarrier.srcAccessMask = 0;
    toTransferDstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransferDstBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toTransferDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDstBarrier.image = swapchainImages_[imageIndex];
    toTransferDstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransferDstBarrier.subresourceRange.baseMipLevel = 0;
    toTransferDstBarrier.subresourceRange.levelCount = 1;
    toTransferDstBarrier.subresourceRange.baseArrayLayer = 0;
    toTransferDstBarrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toTransferDstBarrier);

    VkClearColorValue clearColor = {};
    clearColor.float32[0] = pendingClearColor_[0];
    clearColor.float32[1] = pendingClearColor_[1];
    clearColor.float32[2] = pendingClearColor_[2];
    clearColor.float32[3] = pendingClearColor_[3];
    VkImageSubresourceRange clearRange = {};
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = 1;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = 1;
    vkCmdClearColorImage(
        commandBuffer,
        swapchainImages_[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clearColor,
        1,
        &clearRange);

    VkImageMemoryBarrier toColorAttachmentBarrier = {};
    toColorAttachmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColorAttachmentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toColorAttachmentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toColorAttachmentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toColorAttachmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColorAttachmentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachmentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachmentBarrier.image = swapchainImages_[imageIndex];
    toColorAttachmentBarrier.subresourceRange = clearRange;
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toColorAttachmentBarrier);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = framebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    // Pass 1: game content only (without ImGui)
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!pendingQuads_.empty())
    {
        VkClearAttachment quadAttachment = {};
        quadAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        quadAttachment.colorAttachment = 0;
        quadAttachment.clearValue.color.float32[0] = 0.95f;
        quadAttachment.clearValue.color.float32[1] = 0.65f;
        quadAttachment.clearValue.color.float32[2] = 0.15f;
        quadAttachment.clearValue.color.float32[3] = 1.0f;

        for (const QuadNdc& quad : pendingQuads_)
        {
            const float halfWidth = quad.width * 0.5f;
            const float halfHeight = quad.height * 0.5f;
            const float leftNdc = quad.centerX - halfWidth;
            const float rightNdc = quad.centerX + halfWidth;
            const float bottomNdc = quad.centerY - halfHeight;
            const float topNdc = quad.centerY + halfHeight;

            const float x0f = ((leftNdc * 0.5f) + 0.5f) * static_cast<float>(swapchainExtent_.width);
            const float x1f = ((rightNdc * 0.5f) + 0.5f) * static_cast<float>(swapchainExtent_.width);
            const float y0f = ((1.0f - topNdc) * 0.5f) * static_cast<float>(swapchainExtent_.height);
            const float y1f = ((1.0f - bottomNdc) * 0.5f) * static_cast<float>(swapchainExtent_.height);

            const int32_t x0 = static_cast<int32_t>(std::floor((std::min)(x0f, x1f)));
            const int32_t y0 = static_cast<int32_t>(std::floor((std::min)(y0f, y1f)));
            const int32_t x1 = static_cast<int32_t>(std::ceil((std::max)(x0f, x1f)));
            const int32_t y1 = static_cast<int32_t>(std::ceil((std::max)(y0f, y1f)));

            const int32_t clippedX0 = (std::max)(0, (std::min)(x0, static_cast<int32_t>(swapchainExtent_.width)));
            const int32_t clippedY0 = (std::max)(0, (std::min)(y0, static_cast<int32_t>(swapchainExtent_.height)));
            const int32_t clippedX1 = (std::max)(0, (std::min)(x1, static_cast<int32_t>(swapchainExtent_.width)));
            const int32_t clippedY1 = (std::max)(0, (std::min)(y1, static_cast<int32_t>(swapchainExtent_.height)));

            const uint32_t rectWidth = static_cast<uint32_t>((std::max)(0, clippedX1 - clippedX0));
            const uint32_t rectHeight = static_cast<uint32_t>((std::max)(0, clippedY1 - clippedY0));
            if (rectWidth == 0 || rectHeight == 0)
            {
                continue;
            }

            VkClearRect clearRect = {};
            clearRect.rect.offset = { clippedX0, clippedY0 };
            clearRect.rect.extent = { rectWidth, rectHeight };
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            vkCmdClearAttachments(commandBuffer, 1, &quadAttachment, 1, &clearRect);
        }
    }

    vkCmdEndRenderPass(commandBuffer);
    if (sceneCaptureEnabled_ && sceneCaptureImage_ != VK_NULL_HANDLE)
    {
        RecordSceneCaptureCopy(commandBuffer, imageIndex);
    }

    if (pendingImGuiDrawData_ != nullptr && pendingImGuiDrawData_->CmdListsCount > 0)
    {
        VkImageMemoryBarrier toColorForImGuiBarrier = {};
        toColorForImGuiBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColorForImGuiBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toColorForImGuiBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toColorForImGuiBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toColorForImGuiBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColorForImGuiBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorForImGuiBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorForImGuiBarrier.image = swapchainImages_[imageIndex];
        toColorForImGuiBarrier.subresourceRange = clearRange;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toColorForImGuiBarrier);

        // Pass 2: ImGui overlay only
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(pendingImGuiDrawData_, commandBuffer);
        vkCmdEndRenderPass(commandBuffer);
    }

    vkEndCommandBuffer(commandBuffer);
}
#endif
