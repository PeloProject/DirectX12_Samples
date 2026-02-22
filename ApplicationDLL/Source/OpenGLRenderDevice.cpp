#include "pch.h"
#include "OpenGLRenderDevice.h"

#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

OpenGLRenderDevice::~OpenGLRenderDevice()
{
    Shutdown();
}

void OpenGLRenderDevice::SetPresentBackendLabel(const char* label)
{
    presentBackendLabel_ = (label != nullptr && label[0] != '\0') ? label : "OpenGL";
}

bool OpenGLRenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    LOG_DEBUG("%s RenderDevice::Initialize begin", presentBackendLabel_);
    hwnd_ = hwnd;
    hdc_ = GetDC(hwnd_);
    if (hdc_ == nullptr)
    {
        LOG_DEBUG("%s RenderDevice::Initialize failed: GetDC", presentBackendLabel_);
        return false;
    }

    int currentPixelFormat = GetPixelFormat(hdc_);
    if (currentPixelFormat == 0)
    {
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        const int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
        if (pixelFormat == 0)
        {
            LOG_DEBUG("%s RenderDevice::Initialize failed: ChoosePixelFormat", presentBackendLabel_);
            Shutdown();
            return false;
        }

        if (!SetPixelFormat(hdc_, pixelFormat, &pfd))
        {
            LOG_DEBUG("%s RenderDevice::Initialize failed: SetPixelFormat", presentBackendLabel_);
            Shutdown();
            return false;
        }
    }
    else
    {
        PIXELFORMATDESCRIPTOR currentPfd = {};
        if (DescribePixelFormat(hdc_, currentPixelFormat, sizeof(currentPfd), &currentPfd) == 0)
        {
            LOG_DEBUG("%s RenderDevice::Initialize failed: DescribePixelFormat", presentBackendLabel_);
            Shutdown();
            return false;
        }

        const DWORD requiredFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        if ((currentPfd.dwFlags & requiredFlags) != requiredFlags)
        {
            LOG_DEBUG("%s RenderDevice::Initialize failed: existing pixel format incompatible", presentBackendLabel_);
            Shutdown();
            return false;
        }
    }

    hglrc_ = wglCreateContext(hdc_);
    if (hglrc_ == nullptr)
    {
        LOG_DEBUG("%s RenderDevice::Initialize failed: wglCreateContext", presentBackendLabel_);
        Shutdown();
        return false;
    }

    if (!wglMakeCurrent(hdc_, hglrc_))
    {
        LOG_DEBUG("%s RenderDevice::Initialize failed: wglMakeCurrent", presentBackendLabel_);
        Shutdown();
        return false;
    }

    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    LOG_DEBUG("%s RenderDevice::Initialize success", presentBackendLabel_);
    return true;
}

void OpenGLRenderDevice::Shutdown()
{
    if (hglrc_ != nullptr)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc_);
        hglrc_ = nullptr;
    }

    if (hdc_ != nullptr && hwnd_ != nullptr)
    {
        ReleaseDC(hwnd_, hdc_);
        hdc_ = nullptr;
    }

    hwnd_ = nullptr;
}

bool OpenGLRenderDevice::Resize(UINT width, UINT height)
{
    if (hglrc_ == nullptr || hdc_ == nullptr)
    {
        return false;
    }

    if (!wglMakeCurrent(hdc_, hglrc_))
    {
        return false;
    }

    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    return true;
}

bool OpenGLRenderDevice::PrepareImGuiRenderContext()
{
    if (hglrc_ == nullptr || hdc_ == nullptr)
    {
        return false;
    }
    return wglMakeCurrent(hdc_, hglrc_) == TRUE;
}

void OpenGLRenderDevice::PreRender(const float clearColor[4])
{
    if (hglrc_ == nullptr || hdc_ == nullptr)
    {
        return;
    }

    if (!wglMakeCurrent(hdc_, hglrc_))
    {
        static bool loggedMakeCurrentFailure = false;
        if (!loggedMakeCurrentFailure)
        {
            loggedMakeCurrentFailure = true;
            LOG_DEBUG("%s RenderDevice::PreRender failed: wglMakeCurrent", presentBackendLabel_);
        }
        return;
    }
    const float* c = (clearColor != nullptr) ? clearColor : nullptr;
    RECT clientRect = {};
    if (hwnd_ != nullptr && GetClientRect(hwnd_, &clientRect))
    {
        const GLsizei viewportWidth = static_cast<GLsizei>((std::max)(1L, clientRect.right - clientRect.left));
        const GLsizei viewportHeight = static_cast<GLsizei>((std::max)(1L, clientRect.bottom - clientRect.top));
        glViewport(0, 0, viewportWidth, viewportHeight);
    }
    glClearColor(c ? c[0] : 0.1f, c ? c[1] : 0.1f, c ? c[2] : 0.1f, c ? c[3] : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderDevice::Render()
{
    if (hdc_ != nullptr && hglrc_ != nullptr)
    {
        static uint32_t renderFrameCounter = 0;
        ++renderFrameCounter;
        if (!wglMakeCurrent(hdc_, hglrc_))
        {
            static bool loggedMakeCurrentFailure = false;
            if (!loggedMakeCurrentFailure)
            {
                loggedMakeCurrentFailure = true;
                LOG_DEBUG("%s RenderDevice::Render failed: wglMakeCurrent", presentBackendLabel_);
            }
            return;
        }

        glFlush();
        if ((renderFrameCounter % 120) == 0)
        {
            LOG_DEBUG("%s RenderDevice::Render progress frame=%u",
                presentBackendLabel_,
                renderFrameCounter);
        }
        if ((renderFrameCounter % 240) == 0)
        {
            GLint viewport[4] = {};
            GLint drawBuffer = 0;
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
            LOG_DEBUG("Active present state: backend=%s viewport=(%d,%d,%d,%d) drawBuffer=%d",
                presentBackendLabel_,
                viewport[0], viewport[1], viewport[2], viewport[3], drawBuffer);
        }
        const BOOL swapOk = SwapBuffers(hdc_);
        if (!swapOk)
        {
            static bool loggedSwapFailure = false;
            if (!loggedSwapFailure)
            {
                loggedSwapFailure = true;
                LOG_DEBUG("%s RenderDevice::Render failed: SwapBuffers lastError=%lu",
                    presentBackendLabel_,
                    GetLastError());
            }
        }
    }
}
