#include "pch.h"
#include "RenderDeviceFactory.h"

#include "Dx12RenderDevice.h"
#include "OpenGLRenderDevice.h"
#include "VulkanRenderDevice.h"

std::unique_ptr<IRenderDevice> CreateRenderDevice(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::DirectX12:
        return std::make_unique<Dx12RenderDevice>();
    case RendererBackend::Vulkan:
        return std::make_unique<VulkanRenderDevice>();
    case RendererBackend::OpenGL:
        return std::make_unique<OpenGLRenderDevice>();
    default:
        return nullptr;
    }
}
