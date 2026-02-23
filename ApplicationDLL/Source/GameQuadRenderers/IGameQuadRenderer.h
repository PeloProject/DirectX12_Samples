#pragma once

#include "Source/IRenderDevice.h"

class IGameQuadRenderer
{
public:
    virtual ~IGameQuadRenderer() = default;
    virtual void SetTransform(float centerX, float centerY, float width, float height) = 0;
    virtual void Render(IRenderDevice* renderDevice) = 0;
};
