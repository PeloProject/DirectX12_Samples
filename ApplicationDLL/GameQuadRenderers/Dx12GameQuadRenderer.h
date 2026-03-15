#pragma once

#include "IGameQuadRenderer.h"
#include "PolygonTest.h"

class Dx12GameQuadRenderer final : public IGameQuadRenderer
{
public:
    void SetTransform(float centerX, float centerY, float width, float height) override;
    void SetTextureHandle(TextureHandle textureHandle) override;
    void SetMaterialName(const std::string& materialName) override;
    void Render(IRenderDevice* renderDevice) override;

private:
    QuadRenderObject quad_;
};
