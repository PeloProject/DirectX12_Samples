#pragma once

#include "IGameQuadRenderer.h"

#include <algorithm>

class NdcGameQuadRendererBase : public IGameQuadRenderer
{
public:
    void SetTransform(float centerX, float centerY, float width, float height) override
    {
        centerX_ = centerX;
        centerY_ = centerY;
        width_ = (std::max)(width, 0.01f);
        height_ = (std::max)(height, 0.01f);
    }

    void Render(IRenderDevice* renderDevice) override
    {
        if (renderDevice == nullptr)
        {
            return;
        }
        renderDevice->DrawQuadNdc(centerX_, centerY_, width_, height_);
    }

private:
    float centerX_ = 0.0f;
    float centerY_ = 0.0f;
    float width_ = 0.8f;
    float height_ = 1.4f;
};
