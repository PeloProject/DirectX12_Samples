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
    void DrawQuadNdc(float centerX, float centerY, float width, float height) override;
    bool PrepareImGuiRenderContext() override;
    void SetPresentBackendLabel(const char* label);

private:
    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    HGLRC hglrc_ = nullptr;
    const char* presentBackendLabel_ = "OpenGL";
};
