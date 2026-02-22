#pragma once

#include <Windows.h>
#include "RendererBackend.h"

struct ID3D12CommandQueue;
class IRenderDevice;

struct EditorUiRuntimeState
{
    bool isPieRunning = false;
    uint32_t currentRendererBackend = 0;
    const char* pieGameStatus = "";
    const char* pieGameLastLoadError = "";
    const char* moduleSourceText = "";
    const char* pieGameModulePath = "";
    const char* pieManagedLastPublishLogPath = "";
    int activeQuadCount = 0;
};

struct EditorUiCallbacks
{
    void (*startPie)() = nullptr;
    void (*stopPie)() = nullptr;
    bool (*setRendererBackend)(uint32_t backend) = nullptr;
};

namespace EditorUi
{
    bool Initialize(RendererBackend backend, HWND hwnd, ID3D12CommandQueue* commandQueue, IRenderDevice* renderDevice, UINT initialWidth, UINT initialHeight);
    void Shutdown();
    bool IsInitialized();
    bool HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool WantsMouseCapture();
    void InjectWin32Input(UINT msg, WPARAM wParam, LPARAM lParam);
    void ResetInputState();

    void RequestSceneRenderSize(UINT width, UINT height);
    bool EnsureSceneRenderSize();
    void BeginSceneRenderToTexture(bool isPieRunning, const float* gameClearColor, const float* defaultClearColor);
    void EndSceneRenderToTexture();

    void RenderFrame(bool isStandaloneMode, IRenderDevice* renderDevice, const EditorUiRuntimeState& state, const EditorUiCallbacks& callbacks);
}
