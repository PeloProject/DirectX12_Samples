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
        glScissor(0, 0, viewportWidth, viewportHeight);
    }

    // Reset critical state so previous frame/backend draw state doesn't leak into clear/present.
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);

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

void OpenGLRenderDevice::DrawQuadNdc(float centerX, float centerY, float width, float height)
{
    if (hdc_ == nullptr || hglrc_ == nullptr)
    {
        return;
    }

    if (!wglMakeCurrent(hdc_, hglrc_))
    {
        return;
    }

    const float halfWidth = (std::max)(width * 0.5f, 0.005f);
    const float halfHeight = (std::max)(height * 0.5f, 0.005f);
    const float left = centerX - halfWidth;
    const float right = centerX + halfWidth;
    const float bottom = centerY - halfHeight;
    const float top = centerY + halfHeight;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_TRIANGLES);
    glColor4f(0.95f, 0.65f, 0.15f, 1.0f);
    glVertex2f(left, bottom);
    glVertex2f(left, top);
    glVertex2f(right, bottom);

    glColor4f(0.95f, 0.65f, 0.15f, 1.0f);
    glVertex2f(right, bottom);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
