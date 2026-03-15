#pragma once

#include "ISpriteRendererBackend.h"
#include "Source/RendererBackend.h"

#include <memory>

std::unique_ptr<ISpriteRendererBackend> CreateSpriteRendererBackend(RendererBackend backend);
