#include "pch.h"
#include "VulkanRenderDevice.h"

#include <algorithm>
#include <array>
#include <cstring>

#if APPLICATIONDLL_HAS_VULKAN
#pragma comment(lib, "vulkan-1.lib")

namespace
{
    bool CheckVk(VkResult result)
    {
        return result == VK_SUCCESS;
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
    // Native Vulkan path is temporarily disabled because it can block at runtime on some environments.
    // Keep Vulkan backend operational through OpenGL fallback path.
    useNativeVulkan_ = false;
    LOG_DEBUG("VulkanRenderDevice: native Vulkan path disabled. Using OpenGL fallback present path");
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

            if (commandPool_ != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device_, commandPool_, nullptr);
                commandPool_ = VK_NULL_HANDLE;
                commandBuffers_.clear();
            }

            CleanupSwapchain();
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
        !CreateSyncObjects())
    {
        return false;
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
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    if (renderPass_ != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

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

void VulkanRenderDevice::RecordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkResetCommandBuffer(commandBuffer, 0);
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkClearValue clearValue = {};
    clearValue.color.float32[0] = pendingClearColor_[0];
    clearValue.color.float32[1] = pendingClearColor_[1];
    clearValue.color.float32[2] = pendingClearColor_[2];
    clearValue.color.float32[3] = pendingClearColor_[3];

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = framebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(commandBuffer);
    vkEndCommandBuffer(commandBuffer);
}
#endif
