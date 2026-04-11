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
#include <vector>


using PieGameStartFn = void(__cdecl*)();
using PieGameTickFn = void(__cdecl*)(float);
using PieGameStopFn = void(__cdecl*)();

struct PieNativeApiTable
{
    void(__cdecl* setGameClearColor)(float, float, float, float) = nullptr;
    uint32_t(__cdecl* createSpriteRenderer)() = nullptr;
    void(__cdecl* destroySpriteRenderer)(uint32_t) = nullptr;
    void(__cdecl* setSpriteRendererTransform)(uint32_t, float, float, float, float) = nullptr;
    uint32_t(__cdecl* acquireTextureHandle)(const char*) = nullptr;
    void(__cdecl* releaseTextureHandle)(uint32_t) = nullptr;
    void(__cdecl* setSpriteRendererTexture)(uint32_t, uint32_t) = nullptr;
    void(__cdecl* setSpriteRendererMaterial)(uint32_t, const char*) = nullptr;
};

using PieSetNativeApiFn = void(__cdecl*)(const PieNativeApiTable*);

enum class ViewportRenderMode : uint32_t
{
    Game = 0,
    Scene = 1,
};

struct ViewportCamera2D
{
    float centerX = 0.0f;
    float centerY = 0.0f;
    float zoom = 1.0f;
    float rotationDegrees = 0.0f;
};

struct RuntimeTransform
{
    float location[3] = { 0.0f, 0.0f, 0.0f };
    float rotation[3] = { 0.0f, 0.0f, 0.0f };
    float scale[3] = { 1.0f, 1.0f, 1.0f };
};

struct RuntimeCameraComponent
{
    bool enabled = false;
    float zoom = 1.0f;
};

struct RuntimeActor
{
    std::string name;
    RuntimeTransform transform;
    RuntimeCameraComponent cameraComponent;
};

struct RuntimeState
{
    HWND g_hwnd = NULL;
    HWND g_gameHwnd = NULL;
    bool g_ownsHwnd = true;
    bool g_ownsGameHwnd = true;
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
    ViewportCamera2D g_sceneViewportCamera = { -0.35f, 0.15f, 1.35f, 0.0f };
    ViewportCamera2D g_gameViewportCamera = { 0.0f, 0.0f, 1.0f, 0.0f };
    std::vector<RuntimeActor> g_runtimeActors;
    std::wstring g_windowClassName;
};

class AppRuntime
{
public:
    static AppRuntime& Get();
#pragma region PIE
	PlayInEditor& GetPlayInEditor() { return m_PlayInEditor; }
#pragma endregion

    HWND CreateNativeWindow();
    HWND CreateNativeChildWindow(HWND parentHwnd, UINT width, UINT height);
    HWND CreateGameNativeWindow();
    HWND CreateGameNativeChildWindow(HWND parentHwnd, UINT width, UINT height);
    void ShowNativeWindow();
    void HideNativeWindow();
    void DestroyNativeWindow();
    void DestroyGameNativeWindow();

    void ChangeWindowSize(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


    void SetGameClearColor(float r, float g, float b, float a);
    uint32_t CreateSpriteRenderer();
    void DestroySpriteRenderer(uint32_t handle);
    void SetSpriteRendererTransform(uint32_t handle, float centerX, float centerY, float width, float height);
    void SetSpriteRendererTexture(uint32_t handle, TextureHandle textureHandle);
    void SetSpriteRendererMaterial(uint32_t handle, const char* materialName);
    void SetSceneViewportCamera(float centerX, float centerY, float zoom);
    void GetSceneViewportCamera(float* outCenterX, float* outCenterY, float* outZoom) const;
    void SetSceneViewportRotation(float rotationDegrees);
    float GetSceneViewportRotation() const;
    void SetGameViewportCamera(float centerX, float centerY, float zoom);
    void GetGameViewportCamera(float* outCenterX, float* outCenterY, float* outZoom) const;

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
ViewportRenderMode ResolveViewportRenderMode(HWND hwnd);
ViewportCamera2D GetViewportCamera(ViewportRenderMode mode);
void TransformWorldQuadToViewportNdc(
    ViewportRenderMode mode,
    float centerX,
    float centerY,
    float width,
    float height,
    float& outCenterX,
    float& outCenterY,
    float& outWidth,
    float& outHeight);

extern "C" __declspec(dllexport) void StartPie();
extern "C" __declspec(dllexport) void StopPie();
extern "C" __declspec(dllexport) void SetEditorUiEnabled(BOOL enabled);
extern "C" __declspec(dllexport) BOOL IsEditorUiEnabled();
extern "C" __declspec(dllexport) void SetGameClearColor(float r, float g, float b, float a);
extern "C" __declspec(dllexport) uint32_t CreateSpriteRenderer();
extern "C" __declspec(dllexport) void DestroySpriteRenderer(uint32_t handle);
extern "C" __declspec(dllexport) void SetSpriteRendererTransform(uint32_t handle, float centerX, float centerY, float width, float height);
extern "C" __declspec(dllexport) uint32_t AcquireTextureHandle(const char* texturePath);
extern "C" __declspec(dllexport) void ReleaseTextureHandle(uint32_t textureHandle);
extern "C" __declspec(dllexport) void SetSpriteRendererTexture(uint32_t handle, uint32_t textureHandle);
extern "C" __declspec(dllexport) void SetSpriteRendererMaterial(uint32_t handle, const char* materialName);
extern "C" __declspec(dllexport) void SetSceneViewportCamera(float centerX, float centerY, float zoom);
extern "C" __declspec(dllexport) void GetSceneViewportCamera(float* outCenterX, float* outCenterY, float* outZoom);
extern "C" __declspec(dllexport) void SetSceneViewportRotation(float rotationDegrees);
extern "C" __declspec(dllexport) float GetSceneViewportRotation();
extern "C" __declspec(dllexport) void SetGameViewportCamera(float centerX, float centerY, float zoom);
extern "C" __declspec(dllexport) void GetGameViewportCamera(float* outCenterX, float* outCenterY, float* outZoom);
extern "C" __declspec(dllexport) BOOL SetRendererBackend(uint32_t backend);
extern "C" __declspec(dllexport) uint32_t GetRendererBackend();
extern "C" __declspec(dllexport) HWND GetNativeWindowHandle();
extern "C" __declspec(dllexport) HWND CreateNativeChildWindow(HWND parentHwnd, UINT width, UINT height);
extern "C" __declspec(dllexport) HWND CreateGameNativeWindow();
extern "C" __declspec(dllexport) HWND CreateGameNativeChildWindow(HWND parentHwnd, UINT width, UINT height);
extern "C" __declspec(dllexport) void DestroyGameNativeWindow();
extern "C" __declspec(dllexport) HWND GetGameNativeWindowHandle();
extern "C" __declspec(dllexport) const char* GetRuntimeStatusText();
extern "C" __declspec(dllexport) const char* GetRuntimeLastErrorText();
