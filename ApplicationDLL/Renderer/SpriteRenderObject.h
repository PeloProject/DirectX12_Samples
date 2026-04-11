#pragma once

#include "Source/IRenderDevice.h"
#include "Source/RendererBackend.h"
#include "RHI/TextureAssetManager.h"

#include <memory>
#include <string>
#include <cstdint>

enum class ViewportRenderMode : uint32_t;

class ISpriteRenderObject
{
public:
    virtual ~ISpriteRenderObject() = default;
    virtual void SetTransform(float centerX, float centerY, float width, float height) = 0;
    virtual void SetTextureHandle(TextureHandle textureHandle) = 0;
    virtual void SetMaterialName(const std::string& materialName) = 0;
    virtual void Render(IRenderDevice* renderDevice, ViewportRenderMode viewportMode) = 0;
};

std::unique_ptr<ISpriteRenderObject> CreateSpriteRenderObjectForBackend(RendererBackend backend);
