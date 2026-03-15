#pragma once

#include "Source/IRenderDevice.h"
#include "Source/RendererBackend.h"
#include "RHI/TextureAssetManager.h"

#include <memory>
#include <string>

class ISpriteRenderObject
{
public:
    virtual ~ISpriteRenderObject() = default;
    virtual void SetTransform(float centerX, float centerY, float width, float height) = 0;
    virtual void SetTextureHandle(TextureHandle textureHandle) = 0;
    virtual void SetMaterialName(const std::string& materialName) = 0;
    virtual void Render(IRenderDevice* renderDevice) = 0;
};

std::unique_ptr<ISpriteRenderObject> CreateSpriteRenderObjectForBackend(RendererBackend backend);
