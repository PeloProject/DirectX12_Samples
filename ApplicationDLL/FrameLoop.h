#pragma once

#include <cstdint>

enum class ViewportRenderMode : uint32_t;

void RenderSpriteRenderers(ViewportRenderMode viewportMode);
void DestroyAllSpriteRenderers();
