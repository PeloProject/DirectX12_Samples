#pragma once

#include "Source/IRenderDevice.h"
#include "Source/RendererBackend.h"
#include "GameQuad.h"

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

using PieTickCallback = void(__cdecl*)(float);
using PieGameStartFn = void(__cdecl*)();
using PieGameTickFn = void(__cdecl*)(float);
using PieGameStopFn = void(__cdecl*)();

struct RuntimeState
{
    HWND g_hwnd = NULL;
    std::unique_ptr<IRenderDevice> g_renderDevice;
    bool g_imguiInitialized = false;
    PieTickCallback g_pieTickCallback = nullptr;
    bool g_isPieRunning = false;
    bool g_isStandaloneMode = false;
    bool g_pendingStartPie = false;
    bool g_pendingStopPie = false;
    HMODULE g_pieGameModule = nullptr;
    PieGameStartFn g_pieGameStart = nullptr;
    PieGameTickFn g_pieGameTick = nullptr;
    PieGameStopFn g_pieGameStop = nullptr;
    std::string g_pieGameStatus = "PIE module not loaded";
    std::string g_pieGameModulePath = "";
    std::string g_pieGameSourceModulePath = "";
    std::string g_pieGameLastLoadError = "";
    std::filesystem::file_time_type g_pieGameSourceWriteTime = {};
    bool g_pieGameSourceWriteTimeValid = false;
    std::filesystem::path g_pieManagedCsprojPath = {};
    std::filesystem::path g_pieManagedHotReloadPublishedDllPath = {};
    std::filesystem::file_time_type g_pieManagedLastPublishedSourceWriteTime = {};
    bool g_pieManagedLastPublishedSourceWriteTimeValid = false;
    std::filesystem::file_time_type g_pieManagedPendingPublishSourceWriteTime = {};
    bool g_pieManagedPendingPublishSourceWriteTimeValid = false;
    HANDLE g_pieManagedPublishProcess = nullptr;
    std::string g_pieManagedLastPublishLogPath = "";
    uint64_t g_pieManagedAutoPublishGeneration = 0;
    uint64_t g_pieHotReloadGeneration = 0;
    float g_pieHotReloadCheckTimer = 0.0f;
    float g_pieManagedPublishCheckTimer = 0.0f;
    float g_gameClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    std::unordered_map<uint32_t, std::unique_ptr<IGameQuad>> g_gameQuads;
    uint32_t g_nextGameQuadHandle = 1;
    RendererBackend g_displayRendererBackend = RendererBackend::DirectX12;
    RendererBackend g_rendererBackend = RendererBackend::DirectX12;
    bool g_rendererBackendLocked = false;
    bool g_pendingRendererSwitch = false;
    RendererBackend g_pendingRendererBackend = RendererBackend::DirectX12;
};

class AppRuntime
{
public:
    static AppRuntime& Get();

    HWND CreateNativeWindow();
    void ShowNativeWindow();
    void HideNativeWindow();
    void DestroyNativeWindow();

    void ChangeWindowSize(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void SetPieTickCallback(PieTickCallback callback);
    void RequestStartPie();
    void RequestStopPie();
    void SetStandaloneMode(BOOL enabled);
    BOOL IsPieRunning() const;
    void SetGameClearColor(float r, float g, float b, float a);
    uint32_t CreateGameQuad();
    void DestroyGameQuad(uint32_t handle);
    void SetGameQuadTransform(uint32_t handle, float centerX, float centerY, float width, float height);
    BOOL SetRendererBackend(uint32_t backend);
    uint32_t GetRendererBackend() const;
    bool ApplyPendingRendererSwitch();

    void MessageLoopIteration();
    void UpdatePie();
    LRESULT HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    RuntimeState& MutableState();
    const RuntimeState& State() const;

private:
    AppRuntime() = default;
    RuntimeState state_;
};

AppRuntime& Runtime();
RuntimeState& RuntimeStateRef();

extern "C" __declspec(dllexport) void StartPie();
extern "C" __declspec(dllexport) void StopPie();
extern "C" __declspec(dllexport) BOOL SetRendererBackend(uint32_t backend);
extern "C" __declspec(dllexport) uint32_t GetRendererBackend();
extern "C" __declspec(dllexport) HWND GetNativeWindowHandle();
