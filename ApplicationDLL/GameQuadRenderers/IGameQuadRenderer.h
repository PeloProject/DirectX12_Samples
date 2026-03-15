#pragma once

#include "Source/IRenderDevice.h"
#include "RHI/TextureAssetManager.h"

#include <string>

class IGameQuadRenderer
{
public:
    virtual ~IGameQuadRenderer() = default;
    virtual void SetTransform(float centerX, float centerY, float width, float height) = 0;
    virtual void SetTextureHandle(TextureHandle textureHandle) = 0;
    virtual void SetMaterialName(const std::string& materialName) = 0;
    virtual void Render(IRenderDevice* renderDevice) = 0;
};
