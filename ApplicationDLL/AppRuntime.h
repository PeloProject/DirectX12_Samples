#pragma once

#include "Source/IRenderDevice.h"
#include "RHI/TextureAssetManager.h"
#include "Source/RendererBackend.h"
#include "Renderer/SpriteRenderObject.h"
#include "PlayInEditor.h"

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>


using PieGameStartFn = void(__cdecl*)();
using PieGameTickFn = void(__cdecl*)(float);
using PieGameStopFn = void(__cdecl*)();


struct RuntimeState
{
    HWND g_hwnd = NULL;
    std::unique_ptr<IRenderDevice> g_renderDevice;
    bool g_imguiInitialized = false;
    bool g_editorUiEnabled = true;
    PieTickCallback g_pieTickCallback = nullptr;
    bool g_isStandaloneMode = false;
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
    std::unordered_map<uint32_t, std::unique_ptr<ISpriteRenderObject>> g_spriteRenderers;
    uint32_t g_nextSpriteRendererHandle = 1;
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
#pragma region PIE
	PlayInEditor& GetPlayInEditor() { return m_PlayInEditor; }
#pragma endregion

    HWND CreateNativeWindow();
    void ShowNativeWindow();
    void HideNativeWindow();
    void DestroyNativeWindow();

    void ChangeWindowSize(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


    void SetGameClearColor(float r, float g, float b, float a);
    uint32_t CreateSpriteRenderer();
    void DestroySpriteRenderer(uint32_t handle);
    void SetSpriteRendererTransform(uint32_t handle, float centerX, float centerY, float width, float height);
    void SetSpriteRendererTexture(uint32_t handle, TextureHandle textureHandle);
    void SetSpriteRendererMaterial(uint32_t handle, const char* materialName);

    /// <summary>
	/// テクスチャパスを指定してテクスチャハンドルを取得します。テクスチャがまだロードされていない場合は、非同期にロードが開始されます。
    /// </summary>
    /// <param name="texturePath"></param>
    /// <returns></returns>
    TextureHandle AcquireTextureHandle(const char* texturePath);

    void ReleaseTextureHandle(TextureHandle textureHandle);
    BOOL SetRendererBackend(uint32_t backend);
    uint32_t GetRendererBackend() const;
    bool ApplyPendingRendererSwitch();

    void MessageLoopIteration();
    void SetEditorUiEnabled(BOOL enabled);
    BOOL IsEditorUiEnabled() const;
    const char* GetRuntimeStatusText() const;
    const char* GetRuntimeLastErrorText() const;

    LRESULT HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    RuntimeState& MutableState();
    const RuntimeState& State() const;

private:
    AppRuntime() = default;
    RuntimeState state_;
    PlayInEditor m_PlayInEditor;
};

AppRuntime& Runtime();
RuntimeState& RuntimeStateRef();

extern "C" __declspec(dllexport) void StartPie();
extern "C" __declspec(dllexport) void StopPie();
extern "C" __declspec(dllexport) void SetEditorUiEnabled(BOOL enabled);
extern "C" __declspec(dllexport) BOOL IsEditorUiEnabled();
extern "C" __declspec(dllexport) BOOL SetRendererBackend(uint32_t backend);
extern "C" __declspec(dllexport) uint32_t GetRendererBackend();
extern "C" __declspec(dllexport) HWND GetNativeWindowHandle();
extern "C" __declspec(dllexport) const char* GetRuntimeStatusText();
extern "C" __declspec(dllexport) const char* GetRuntimeLastErrorText();
