#pragma once

#include <cstdint>

enum class ViewportRenderMode : uint32_t;

class SceneBase
{
public:
    virtual ~SceneBase() {}
    virtual void Update(float deltaTime) = 0;
    virtual void Render(ViewportRenderMode viewportMode) = 0;
};
