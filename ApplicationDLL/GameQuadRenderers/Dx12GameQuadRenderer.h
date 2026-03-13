#pragma once

#include "IGameQuadRenderer.h"
#include "PolygonTest.h"

class Dx12GameQuadRenderer final : public IGameQuadRenderer
{
public:
    void SetTransform(float centerX, float centerY, float width, float height) override;
    void Render(IRenderDevice* renderDevice) override;

private:
    PolygonTest quad_;
};
