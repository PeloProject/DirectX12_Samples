#include "pch.h"
#include "SpriteRendererBackendFactory.h"

#include "Dx12SpriteRendererBackend.h"
#include "OpenGlSpriteRendererBackend.h"
#include "VulkanSpriteRendererBackend.h"

std::unique_ptr<ISpriteRendererBackend> CreateSpriteRendererBackend(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::DirectX12:
        return std::make_unique<Dx12SpriteRendererBackend>();
    case RendererBackend::Vulkan:
        return std::make_unique<VulkanSpriteRendererBackend>();
    case RendererBackend::OpenGL:
        return std::make_unique<OpenGlSpriteRendererBackend>();
    default:
        return nullptr;
    }
}
