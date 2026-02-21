#include "pch.h"

#include "FrameLoop.h"

#include "AppRuntime.h"
#include "PieAutoPublish.h"
#include "PieLoader.h"
#include "WinHandleRAII.h"

#include "SceneManager.h"
#include "Source/EditorUi.h"

#include <string>
#include <tchar.h>

namespace
{
    constexpr float kDefaultSceneClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    void BeginSceneRenderToTexture()
    {
        EditorUi::BeginSceneRenderToTexture(RuntimeStateRef().g_isPieRunning, RuntimeStateRef().g_gameClearColor, kDefaultSceneClearColor);
    }

    void EndSceneRenderToTexture()
    {
        EditorUi::EndSceneRenderToTexture();
    }
}

void RenderGameQuads()
{
    for (auto& quadEntry : RuntimeStateRef().g_gameQuads)
    {
        if (quadEntry.second != nullptr)
        {
            quadEntry.second->Render();
        }
    }
}

void DestroyAllGameQuads()
{
    RuntimeStateRef().g_gameQuads.clear();
    RuntimeStateRef().g_nextGameQuadHandle = 1;
}

void AppRuntime::SetPieTickCallback(PieTickCallback callback)
{
    RuntimeStateRef().g_pieTickCallback = callback;
}

void AppRuntime::RequestStartPie()
{
    RuntimeStateRef().g_pendingStopPie = false;
    RuntimeStateRef().g_pendingStartPie = true;
}

void AppRuntime::RequestStopPie()
{
    RuntimeStateRef().g_pendingStartPie = false;
    RuntimeStateRef().g_pendingStopPie = true;
}

void AppRuntime::SetStandaloneMode(BOOL enabled)
{
    RuntimeStateRef().g_isStandaloneMode = (enabled == TRUE);
    if (RuntimeStateRef().g_hwnd != NULL)
    {
        SetWindowText(RuntimeStateRef().g_hwnd, RuntimeStateRef().g_isStandaloneMode ? _T("PieGameManaged Player") : _T("Native Window"));
    }
}

///====================================================
/// @brief PIE開始
///====================================================
void StartPieImmediate()
{
    if (!EnsurePieGameModuleLoaded())
    {
        RuntimeStateRef().g_isPieRunning = false;
        return;
    }

    const std::string statusBeforeStart = RuntimeStateRef().g_pieGameStatus;
    RuntimeStateRef().g_pieGameStart();
    RuntimeStateRef().g_isPieRunning = true;
    RuntimeStateRef().g_pieHotReloadCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPublishCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid = false;
    std::filesystem::file_time_type initialSourceWriteTime = {};
    if (TryGetPieManagedSourceWriteTime(initialSourceWriteTime))
    {
        RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTime = initialSourceWriteTime;
        RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTimeValid = true;
    }
    if (RuntimeStateRef().g_gameQuads.empty() && RuntimeStateRef().g_pieGameStatus == statusBeforeStart)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE running (C#) - GameStart created no quads (no native error reported)";
        return;
    }
    if (RuntimeStateRef().g_pieGameStatus == statusBeforeStart)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE running (C#)";
    }
}

///====================================================
/// @brief PIE停止
///====================================================
void StopPieImmediate()
{
    if (RuntimeStateRef().g_isPieRunning && RuntimeStateRef().g_pieGameStop != nullptr)
    {
        RuntimeStateRef().g_pieGameStop();
    }
    DestroyAllGameQuads();
    RuntimeStateRef().g_isPieRunning = false;
    RuntimeStateRef().g_pieHotReloadCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPublishCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid = false;

    ScopedHandle publishProcess(RuntimeStateRef().g_pieManagedPublishProcess);
    RuntimeStateRef().g_pieManagedPublishProcess = nullptr;

    UnloadPieGameModule();
    RuntimeStateRef().g_pieGameStatus = "PIE stopped";
}

BOOL AppRuntime::IsPieRunning() const
{
    return RuntimeStateRef().g_isPieRunning ? TRUE : FALSE;
}

void AppRuntime::SetGameClearColor(float r, float g, float b, float a)
{
    RuntimeStateRef().g_gameClearColor[0] = r;
    RuntimeStateRef().g_gameClearColor[1] = g;
    RuntimeStateRef().g_gameClearColor[2] = b;
    RuntimeStateRef().g_gameClearColor[3] = a;
}

uint32_t AppRuntime::CreateGameQuad()
{
    try
    {
        const uint32_t handle = RuntimeStateRef().g_nextGameQuadHandle++;
        RuntimeStateRef().g_gameQuads[handle] = std::make_unique<PolygonTest>();
        RuntimeStateRef().g_pieGameStatus = "Game quad created. handle=" + std::to_string(handle);
        return handle;
    }
    catch (const std::exception& ex)
    {
        RuntimeStateRef().g_pieGameStatus = std::string("CreateGameQuad failed: ") + ex.what();
        return 0;
    }
    catch (...)
    {
        RuntimeStateRef().g_pieGameStatus = "CreateGameQuad failed: unknown error";
        return 0;
    }
}

void AppRuntime::DestroyGameQuad(uint32_t handle)
{
    if (handle == 0)
    {
        return;
    }

    RuntimeStateRef().g_gameQuads.erase(handle);
}

void AppRuntime::SetGameQuadTransform(uint32_t handle, float centerX, float centerY, float width, float height)
{
    const auto it = RuntimeStateRef().g_gameQuads.find(handle);
    if (it == RuntimeStateRef().g_gameQuads.end() || it->second == nullptr)
    {
        return;
    }

    it->second->SetTransform(centerX, centerY, width, height);
}

///==========================================================
/// @brief メッセージループ
///==========================================================
void AppRuntime::MessageLoopIteration()
{
    if (RuntimeStateRef().g_hwnd == NULL)
    {
        return;
    }

    MSG msg = {};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UpdatePie();

    constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    SceneManager::GetInstance().Update(kFixedDeltaTime);
    TickPieManagedAutoPublish(kFixedDeltaTime);

    if (RuntimeStateRef().g_isPieRunning)
    {
        RuntimeStateRef().g_pieHotReloadCheckTimer += kFixedDeltaTime;
        if (RuntimeStateRef().g_pieHotReloadCheckTimer >= 0.5f)
        {
            RuntimeStateRef().g_pieHotReloadCheckTimer = 0.0f;
            TryHotReloadPieGameModule();
        }
    }

    if (RuntimeStateRef().g_imguiInitialized)
    {
        EditorUi::EnsureSceneRenderSize();
    }

    BeginSceneRenderToTexture();
    SceneManager::GetInstance().Render();
    RenderGameQuads();
    EndSceneRenderToTexture();

    RuntimeStateRef().g_DxDevice.PreRender();

    if (RuntimeStateRef().g_imguiInitialized)
    {
        const std::string moduleSourceText = RuntimeStateRef().g_pieGameSourceModulePath.empty()
            ? (RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath.empty() ? "(none)" : RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath.u8string())
            : RuntimeStateRef().g_pieGameSourceModulePath;

        EditorUiRuntimeState uiState = {};
        uiState.isPieRunning = RuntimeStateRef().g_isPieRunning;
        uiState.pieGameStatus = RuntimeStateRef().g_pieGameStatus.c_str();
        uiState.pieGameLastLoadError = RuntimeStateRef().g_pieGameLastLoadError.c_str();
        uiState.moduleSourceText = moduleSourceText.c_str();
        uiState.pieGameModulePath = RuntimeStateRef().g_pieGameModulePath.c_str();
        uiState.pieManagedLastPublishLogPath = RuntimeStateRef().g_pieManagedLastPublishLogPath.c_str();
        uiState.activeQuadCount = static_cast<int>(RuntimeStateRef().g_gameQuads.size());

        EditorUiCallbacks uiCallbacks = {};
        uiCallbacks.startPie = &StartPie;
        uiCallbacks.stopPie = &StopPie;

        EditorUi::RenderFrame(RuntimeStateRef().g_isStandaloneMode, uiState, uiCallbacks);
    }

    RuntimeStateRef().g_DxDevice.Render();
}

void AppRuntime::UpdatePie()
{
    // PIE停止
    if (RuntimeStateRef().g_pendingStopPie)
    {
        RuntimeStateRef().g_pendingStopPie = false;
        StopPieImmediate();
    }

    // PIE開始
    if (RuntimeStateRef().g_pendingStartPie)
    {
        RuntimeStateRef().g_pendingStartPie = false;
        StartPieImmediate();
    }

    constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    if (RuntimeStateRef().g_isPieRunning && RuntimeStateRef().g_pieGameTick != nullptr)
    {
        RuntimeStateRef().g_pieGameTick(kFixedDeltaTime);
    }
    if (RuntimeStateRef().g_isPieRunning && RuntimeStateRef().g_pieTickCallback != nullptr)
    {
        RuntimeStateRef().g_pieTickCallback(kFixedDeltaTime);
    }
}