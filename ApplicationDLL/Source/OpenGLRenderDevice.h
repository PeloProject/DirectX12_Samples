#pragma once

#include "IRenderDevice.h"

class OpenGLRenderDevice final : public IRenderDevice
{
public:
    OpenGLRenderDevice() = default;
    ~OpenGLRenderDevice() override;

    RendererBackend Backend() const override { return RendererBackend::OpenGL; }

    bool Initialize(HWND hwnd, UINT width, UINT height) override;
    void Shutdown() override;
    bool Resize(UINT width, UINT height) override;
    void PreRender(const float clearColor[4]) override;
    void Render() override;

private:
    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    HGLRC hglrc_ = nullptr;
};
