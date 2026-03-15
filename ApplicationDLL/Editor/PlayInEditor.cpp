#include "pch.h"
#include "PlayInEditor.h"
#include "../AppRuntime.h"
#include "PieLoader.h"
#include "PieAutoPublish.h"
#include "FrameLoop.h"
#include "WinHandleRAII.h"
#include <tchar.h>

PlayInEditor::PlayInEditor()
{

}

PlayInEditor::~PlayInEditor()
{

}

///========================================================
/// @brief PIEのTickのコールバックを設定
///========================================================
void PlayInEditor::SetPieTickCallback(PieTickCallback callback)
{
    RuntimeStateRef().g_pieTickCallback = callback;
}

///========================================================
/// @brief PIEを開始
///========================================================
void PlayInEditor::RequestStartPie()
{
    m_IsStopRequest = false;
    m_IsStartRequest = true;
}

///========================================================
/// @brief PIEを停止
///========================================================
void PlayInEditor::RequestStopPie()
{
    m_IsStartRequest = false;
    m_IsStopRequest = true;
}

///========================================================
/// @brief スタンドアローンモードの設定
/// @param enabled 
///========================================================
void PlayInEditor::SetStandaloneMode(BOOL enabled)
{
    RuntimeStateRef().g_isStandaloneMode = (enabled == TRUE);
    if (RuntimeStateRef().g_hwnd != NULL)
    {
        SetWindowText(RuntimeStateRef().g_hwnd, RuntimeStateRef().g_isStandaloneMode ? _T("PieGameManaged Player") : _T("Native Window"));
    }
}


///====================================================
/// @brief PIE停止
///====================================================
void PlayInEditor::StopImmediate()
{
    if (m_IsRunning && RuntimeStateRef().g_pieGameStop != nullptr)
    {
        RuntimeStateRef().g_pieGameStop();
    }
    DestroyAllSpriteRenderers();
    m_IsRunning = false;
    RuntimeStateRef().g_pieHotReloadCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPublishCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid = false;

    ScopedHandle publishProcess(RuntimeStateRef().g_pieManagedPublishProcess);
    RuntimeStateRef().g_pieManagedPublishProcess = nullptr;

    UnloadPieGameModule();
    RuntimeStateRef().g_pieGameStatus = "PIE stopped";
}

///=====================================================
/// @brief PIE起動中か？
/// @return 
///=====================================================
BOOL PlayInEditor::IsPieRunning() const
{
    return m_IsRunning ? TRUE : FALSE;
}

///=====================================================
/// <summary>
/// PIE（Play In Editor）の状態を更新します。保留中の停止/開始フラグを処理して即時停止・開始を行い、
/// PIEが実行中の場合は固定デルタタイム（1/60秒）で g_pieGameTick および g_pieTickCallback を呼び出します。
/// </summary>
///=====================================================
void PlayInEditor::UpdatePie()
{
    // PIE停止
    if (m_IsStopRequest)
    {
        m_IsStopRequest = false;
        StopImmediate();
    }

    // PIE開始
    if (m_IsStartRequest)
    {
        m_IsStartRequest = false;
        StartImmediate();
    }

	// PIEのTick更新
    constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    if (m_IsRunning && RuntimeStateRef().g_pieGameTick != nullptr)
    {
        RuntimeStateRef().g_pieGameTick(kFixedDeltaTime);
    }
    if (m_IsRunning && RuntimeStateRef().g_pieTickCallback != nullptr)
    {
        RuntimeStateRef().g_pieTickCallback(kFixedDeltaTime);
    }
}


///====================================================
/// @brief PIE開始
///====================================================
void PlayInEditor::StartImmediate()
{
    if (!EnsurePieGameModuleLoaded())
    {
        m_IsRunning = false;
        return;
    }

    const std::string statusBeforeStart = RuntimeStateRef().g_pieGameStatus;
    RuntimeStateRef().g_pieGameStart();
    m_IsRunning = true;
    RuntimeStateRef().g_pieHotReloadCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPublishCheckTimer = 0.0f;
    RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid = false;
    std::filesystem::file_time_type initialSourceWriteTime = {};
    if (TryGetPieManagedSourceWriteTime(initialSourceWriteTime))
    {
        RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTime = initialSourceWriteTime;
        RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTimeValid = true;
    }
    const size_t totalSpriteRendererCount = RuntimeStateRef().g_spriteRenderers.size();
    if (totalSpriteRendererCount == 0 && RuntimeStateRef().g_pieGameStatus == statusBeforeStart)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE running (C#) - GameStart created no SpriteRenderer instances (no native error reported)";
        return;
    }
    if (RuntimeStateRef().g_pieGameStatus == statusBeforeStart)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE running (C#)";
    }
}
