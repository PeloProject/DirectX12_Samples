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
    void CaptureEditorSceneTexture(UINT width, UINT height) override;
    bool HasEditorSceneTexture() const override { return editorSceneTexture_ != 0; }
    uintptr_t GetEditorSceneTextureHandle() const override { return static_cast<uintptr_t>(editorSceneTexture_); }
    void SetPresentBackendLabel(const char* label);

private:
    bool EnsureEditorSceneTexture(UINT width, UINT height);

    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    HGLRC hglrc_ = nullptr;
    unsigned int editorSceneTexture_ = 0;
    UINT editorSceneTextureWidth_ = 0;
    UINT editorSceneTextureHeight_ = 0;
    const char* presentBackendLabel_ = "OpenGL";
};
