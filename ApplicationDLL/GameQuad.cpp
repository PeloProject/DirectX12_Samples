#include "pch.h"
#include "GameQuad.h"

#include "PolygonTest.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace
{
    struct QuadVertex
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
    };

    struct QuadTransform
    {
        float centerX = 0.0f;
        float centerY = 0.0f;
        float width = 0.8f;
        float height = 1.4f;
    };

    class NdcIndexedQuadGameQuadBase : public IGameQuad
    {
    public:
        NdcIndexedQuadGameQuadBase()
        {
            indices_ = { 0, 1, 2, 2, 1, 3 };
            UpdateMesh();
        }

        void SetTransform(float centerX, float centerY, float width, float height) override
        {
            transform_.centerX = centerX;
            transform_.centerY = centerY;
            transform_.width = (std::max)(width, 0.01f);
            transform_.height = (std::max)(height, 0.01f);
            meshDirty_ = true;
        }

        void Render(IRenderDevice* renderDevice) override
        {
            if (renderDevice == nullptr)
            {
                return;
            }

            if (meshDirty_)
            {
                UpdateMesh();
            }

            const float left = vertices_[0].x;
            const float right = vertices_[3].x;
            const float bottom = vertices_[0].y;
            const float top = vertices_[3].y;
            const float centerX = (left + right) * 0.5f;
            const float centerY = (bottom + top) * 0.5f;
            const float width = right - left;
            const float height = top - bottom;

            renderDevice->DrawQuadNdc(centerX, centerY, width, height);
        }

    private:
        void UpdateMesh()
        {
            const float halfWidth = transform_.width * 0.5f;
            const float halfHeight = transform_.height * 0.5f;
            const float left = transform_.centerX - halfWidth;
            const float right = transform_.centerX + halfWidth;
            const float bottom = transform_.centerY - halfHeight;
            const float top = transform_.centerY + halfHeight;

            vertices_[0] = { left, bottom, 0.0f, 0.0f, 1.0f };
            vertices_[1] = { left, top, 0.0f, 0.0f, 0.0f };
            vertices_[2] = { right, bottom, 0.0f, 1.0f, 1.0f };
            vertices_[3] = { right, top, 0.0f, 1.0f, 0.0f };
            meshDirty_ = false;
        }

    private:
        QuadTransform transform_ = {};
        std::array<QuadVertex, 4> vertices_ = {};
        std::array<uint16_t, 6> indices_ = {};
        bool meshDirty_ = true;
    };

    class Dx12GameQuad final : public IGameQuad
    {
    public:
        Dx12GameQuad() = default;

        void SetTransform(float centerX, float centerY, float width, float height) override
        {
            quad_.SetTransform(centerX, centerY, width, height);
        }

        void Render(IRenderDevice* renderDevice) override
        {
            (void)renderDevice;
            quad_.Render();
        }

    private:
        PolygonTest quad_;
    };

    class OpenGlGameQuad final : public NdcIndexedQuadGameQuadBase
    {
    };

    class VulkanGameQuad final : public NdcIndexedQuadGameQuadBase
    {
    };
}

std::unique_ptr<IGameQuad> CreateGameQuadForBackend(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::DirectX12:
        return std::make_unique<Dx12GameQuad>();
    case RendererBackend::Vulkan:
        return std::make_unique<VulkanGameQuad>();
    case RendererBackend::OpenGL:
        return std::make_unique<OpenGlGameQuad>();
    default:
        return nullptr;
    }
}
