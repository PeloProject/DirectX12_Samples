#include "pch.h"
#include "Dx12SpriteRendererBackend.h"

void Dx12SpriteRendererBackend::SetTransform(float centerX, float centerY, float width, float height)
{
    quad_.SetTransform(centerX, centerY, width, height);
}

void Dx12SpriteRendererBackend::SetTextureHandle(TextureHandle textureHandle)
{
    quad_.SetTextureHandle(textureHandle);
}

void Dx12SpriteRendererBackend::SetMaterialName(const std::string& materialName)
{
    quad_.SetMaterialName(materialName);
}

void Dx12SpriteRendererBackend::Render(IRenderDevice* renderDevice, ViewportRenderMode viewportMode)
{
    (void)renderDevice;
    quad_.Render(viewportMode);
}
