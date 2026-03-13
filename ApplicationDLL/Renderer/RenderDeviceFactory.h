#pragma once

#include "IRenderDevice.h"

#include <memory>

std::unique_ptr<IRenderDevice> CreateRenderDevice(RendererBackend backend);
