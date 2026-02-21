#include "pch.h"
#include "RendererBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

const char* RendererBackendToString(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::DirectX12:
        return "DirectX12";
    case RendererBackend::Vulkan:
        return "Vulkan";
    case RendererBackend::OpenGL:
        return "OpenGL";
    default:
        return "Unknown";
    }
}

bool TryParseRendererBackend(const char* text, RendererBackend& outBackend)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }

    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (value == "dx12" || value == "d3d12" || value == "directx12")
    {
        outBackend = RendererBackend::DirectX12;
        return true;
    }
    if (value == "vk" || value == "vulkan")
    {
        outBackend = RendererBackend::Vulkan;
        return true;
    }
    if (value == "gl" || value == "opengl")
    {
        outBackend = RendererBackend::OpenGL;
        return true;
    }

    return false;
}

RendererBackend ResolveRendererBackendFromEnvironment(RendererBackend fallback)
{
    char* value = nullptr;
    size_t len = 0;
    const errno_t err = _dupenv_s(&value, &len, "APP_RENDERER_BACKEND");

    const char* valuePtr = (err == 0) ? value : nullptr;
    RendererBackend parsed = fallback;
    if (TryParseRendererBackend(valuePtr, parsed))
    {
        if (value != nullptr)
        {
            free(value);
        }
        return parsed;
    }

    if (value != nullptr)
    {
        free(value);
    }
    return fallback;
}
