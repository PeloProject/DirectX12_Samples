#include "pch.h"
#include "OpenGLRenderDevice.h"

#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

OpenGLRenderDevice::~OpenGLRenderDevice()
{
    Shutdown();
}

bool OpenGLRenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    hwnd_ = hwnd;
    hdc_ = GetDC(hwnd_);
    if (hdc_ == nullptr)
    {
        return false;
    }

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
        Shutdown();
        return false;
    }

    if (!SetPixelFormat(hdc_, pixelFormat, &pfd))
    {
        Shutdown();
        return false;
    }

    hglrc_ = wglCreateContext(hdc_);
    if (hglrc_ == nullptr)
    {
        Shutdown();
        return false;
    }

    if (!wglMakeCurrent(hdc_, hglrc_))
    {
        Shutdown();
        return false;
    }

    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
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

void OpenGLRenderDevice::PreRender(const float clearColor[4])
{
    if (hglrc_ == nullptr || hdc_ == nullptr)
    {
        return;
    }

    wglMakeCurrent(hdc_, hglrc_);
    const float* c = (clearColor != nullptr) ? clearColor : nullptr;
    glClearColor(c ? c[0] : 0.1f, c ? c[1] : 0.1f, c ? c[2] : 0.1f, c ? c[3] : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderDevice::Render()
{
    if (hdc_ != nullptr)
    {
        SwapBuffers(hdc_);
    }
}
