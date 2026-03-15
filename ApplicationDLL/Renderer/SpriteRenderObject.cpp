#include "pch.h"
#include "SpriteRenderObject.h"

#include "SpriteRenderers/SpriteRendererBackendFactory.h"

#include <memory>

namespace
{
    class SpriteRenderObjectAdapter final : public ISpriteRenderObject
    {
    public:
        explicit SpriteRenderObjectAdapter(std::unique_ptr<ISpriteRendererBackend> renderer)
            : renderer_(std::move(renderer))
        {
        }

        void SetTransform(float centerX, float centerY, float width, float height) override
        {
            if (renderer_ == nullptr)
            {
                return;
            }
            renderer_->SetTransform(centerX, centerY, width, height);
        }

        void SetTextureHandle(TextureHandle textureHandle) override
        {
            if (renderer_ == nullptr)
            {
                return;
            }
            renderer_->SetTextureHandle(textureHandle);
        }

        void SetMaterialName(const std::string& materialName) override
        {
            if (renderer_ == nullptr)
            {
                return;
            }
            renderer_->SetMaterialName(materialName);
        }

        ///===================================================
        /// @brief 描画
        /// @param renderDevice 
        ///===================================================
        void Render(IRenderDevice* renderDevice) override
        {
            if (renderer_ == nullptr)
            {
                return;
            }
            renderer_->Render(renderDevice);
        }

    private:
        std::unique_ptr<ISpriteRendererBackend> renderer_;
    };
}

std::unique_ptr<ISpriteRenderObject> CreateSpriteRenderObjectForBackend(RendererBackend backend)
{
    std::unique_ptr<ISpriteRendererBackend> renderer = CreateSpriteRendererBackend(backend);
    if (renderer == nullptr)
    {
        return nullptr;
    }
    return std::make_unique<SpriteRenderObjectAdapter>(std::move(renderer));
}
