#pragma once

#include <cstdint>
#include <string>

enum class RendererBackend : uint32_t
{
    DirectX12 = 0,
    Vulkan = 1,
    OpenGL = 2,
};

const char* RendererBackendToString(RendererBackend backend);
bool TryParseRendererBackend(const char* text, RendererBackend& outBackend);
RendererBackend ResolveRendererBackendFromEnvironment(RendererBackend fallback);
