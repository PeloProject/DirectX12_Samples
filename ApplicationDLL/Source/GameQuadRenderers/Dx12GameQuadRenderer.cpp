#include "pch.h"
#include "Dx12GameQuadRenderer.h"

void Dx12GameQuadRenderer::SetTransform(float centerX, float centerY, float width, float height)
{
    quad_.SetTransform(centerX, centerY, width, height);
}

void Dx12GameQuadRenderer::Render(IRenderDevice* renderDevice)
{
    (void)renderDevice;
    quad_.Render();
}
