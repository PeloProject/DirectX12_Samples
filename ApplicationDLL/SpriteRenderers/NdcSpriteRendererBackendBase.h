#pragma once

#include "ISpriteRendererBackend.h"
#include "AppRuntime.h"

#include <algorithm>

class NdcSpriteRendererBackendBase : public ISpriteRendererBackend
{
public:
    void SetTransform(float centerX, float centerY, float width, float height) override
    {
        centerX_ = centerX;
        centerY_ = centerY;
        width_ = (std::max)(width, 0.01f);
        height_ = (std::max)(height, 0.01f);
    }

    void Render(IRenderDevice* renderDevice, ViewportRenderMode viewportMode) override
    {
        if (renderDevice == nullptr)
        {
            return;
        }
        float transformedCenterX = centerX_;
        float transformedCenterY = centerY_;
        float transformedWidth = width_;
        float transformedHeight = height_;
        TransformWorldQuadToViewportNdc(
            viewportMode,
            centerX_,
            centerY_,
            width_,
            height_,
            transformedCenterX,
            transformedCenterY,
            transformedWidth,
            transformedHeight);
        renderDevice->DrawQuadNdc(transformedCenterX, transformedCenterY, transformedWidth, transformedHeight);
    }

    void SetTextureHandle(TextureHandle textureHandle) override
    {
        (void)textureHandle;
    }

    void SetMaterialName(const std::string& materialName) override
    {
        (void)materialName;
    }

private:
    float centerX_ = 0.0f;
    float centerY_ = 0.0f;
    float width_ = 0.8f;
    float height_ = 1.4f;
};
