#include "pch.h"
#include "OpenGLLoader.h"

bool InitializeOpenGLLoader()
{
#ifdef PIE_HAS_GLAD
    return gladLoadGL() != 0;
#else
    return true;
#endif
}

const char* GetOpenGLLoaderName()
{
#ifdef PIE_HAS_GLAD
    return "GLAD";
#else
    return "System OpenGL (no GLAD)";
#endif
}
