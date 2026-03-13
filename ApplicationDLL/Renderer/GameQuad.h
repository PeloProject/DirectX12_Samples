#pragma once

#include "Source/IRenderDevice.h"
#include "Source/RendererBackend.h"

#include <memory>

class IGameQuad
{
public:
    virtual ~IGameQuad() = default;
    virtual void SetTransform(float centerX, float centerY, float width, float height) = 0;
    virtual void Render(IRenderDevice* renderDevice) = 0;
};

std::unique_ptr<IGameQuad> CreateGameQuadForBackend(RendererBackend backend);
