#pragma once

// Prefer bundled GLAD when available, otherwise keep the legacy OpenGL path.
#if defined(__has_include)
#if __has_include("../ThirdParty/glad/include/glad/glad.h")
#define PIE_HAS_GLAD 1
#include "../ThirdParty/glad/include/glad/glad.h"
#endif
#endif

#ifndef PIE_HAS_GLAD
#include <gl/GL.h>
#endif

bool InitializeOpenGLLoader();
const char* GetOpenGLLoaderName();
