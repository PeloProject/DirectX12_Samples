#include "pch.h"
#include "GameQuadRendererFactory.h"

#include "Dx12GameQuadRenderer.h"
#include "OpenGlGameQuadRenderer.h"
#include "VulkanGameQuadRenderer.h"

std::unique_ptr<IGameQuadRenderer> CreateGameQuadRenderer(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::DirectX12:
        return std::make_unique<Dx12GameQuadRenderer>();
    case RendererBackend::Vulkan:
        return std::make_unique<VulkanGameQuadRenderer>();
    case RendererBackend::OpenGL:
        return std::make_unique<OpenGlGameQuadRenderer>();
    default:
        return nullptr;
    }
}
