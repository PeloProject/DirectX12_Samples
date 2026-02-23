#include "pch.h"
#include "GameQuad.h"

#include "Source/GameQuadRenderers/GameQuadRendererFactory.h"

#include <memory>

namespace
{
    class GameQuadAdapter final : public IGameQuad
    {
    public:
        explicit GameQuadAdapter(std::unique_ptr<IGameQuadRenderer> renderer)
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

        void Render(IRenderDevice* renderDevice) override
        {
            if (renderer_ == nullptr)
            {
                return;
            }
            renderer_->Render(renderDevice);
        }

    private:
        std::unique_ptr<IGameQuadRenderer> renderer_;
    };
}

std::unique_ptr<IGameQuad> CreateGameQuadForBackend(RendererBackend backend)
{
    std::unique_ptr<IGameQuadRenderer> renderer = CreateGameQuadRenderer(backend);
    if (renderer == nullptr)
    {
        return nullptr;
    }
    return std::make_unique<GameQuadAdapter>(std::move(renderer));
}
