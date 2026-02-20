#pragma once

#include <Windows.h>

struct ID3D12CommandQueue;

struct EditorUiRuntimeState
{
    bool isPieRunning = false;
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
};

namespace EditorUi
{
    bool Initialize(HWND hwnd, ID3D12CommandQueue* commandQueue, UINT initialWidth, UINT initialHeight);
    void Shutdown();
    bool IsInitialized();
    bool HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RequestSceneRenderSize(UINT width, UINT height);
    bool EnsureSceneRenderSize();
    void BeginSceneRenderToTexture(bool isPieRunning, const float* gameClearColor, const float* defaultClearColor);
    void EndSceneRenderToTexture();

    void RenderFrame(bool isStandaloneMode, const EditorUiRuntimeState& state, const EditorUiCallbacks& callbacks);
}
