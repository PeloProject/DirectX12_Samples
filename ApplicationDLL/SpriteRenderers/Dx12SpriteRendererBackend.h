#pragma once

#include "ISpriteRendererBackend.h"
#include "PolygonTest.h"

class Dx12SpriteRendererBackend final : public ISpriteRendererBackend
{
public:
    void SetTransform(float centerX, float centerY, float width, float height) override;
    void SetTextureHandle(TextureHandle textureHandle) override;
    void SetMaterialName(const std::string& materialName) override;
    void Render(IRenderDevice* renderDevice, ViewportRenderMode viewportMode) override;

private:
    QuadRenderObject quad_;
};
