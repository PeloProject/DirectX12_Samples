#pragma once

#include "Source/IRenderDevice.h"
#include "RHI/TextureAssetManager.h"

#include <cstdint>
#include <string>

enum class ViewportRenderMode : uint32_t;

class ISpriteRendererBackend
{
public:
    virtual ~ISpriteRendererBackend() = default;
    virtual void SetTransform(float centerX, float centerY, float width, float height) = 0;
    virtual void SetTextureHandle(TextureHandle textureHandle) = 0;
    virtual void SetMaterialName(const std::string& materialName) = 0;
    virtual void Render(IRenderDevice* renderDevice, ViewportRenderMode viewportMode) = 0;
};
