#pragma once

#include "IGameQuadRenderer.h"
#include "Source/RendererBackend.h"

#include <memory>

std::unique_ptr<IGameQuadRenderer> CreateGameQuadRenderer(RendererBackend backend);
