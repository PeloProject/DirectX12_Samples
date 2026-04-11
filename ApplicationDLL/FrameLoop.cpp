#include "pch.h"

#include "FrameLoop.h"

#include "AppRuntime.h"
#include "PieAutoPublish.h"
#include "PieLoader.h"
#include "RHI/TextureAssetManager.h"
#include "WinHandleRAII.h"

#include "SceneManager.h"
#include "Source/EditorUi.h"
#include "Source/RendererBackend.h"

#include <string>
#include <tchar.h>
#include <algorithm>
#include <unordered_map>

namespace
{
    constexpr float kDefaultSceneClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    std::unordered_map<HWND, SIZE> g_viewportSizes;

    bool SetRendererBackendFromEditorUi(uint32_t backend)
    {
        return Runtime().SetRendererBackend(backend) == TRUE;
    }

    void BeginSceneRenderToTexture()
    {
        EditorUi::BeginSceneRenderToTexture(AppRuntime::Get().GetPlayInEditor().IsPieRunning(), RuntimeStateRef().g_gameClearColor, kDefaultSceneClearColor);
    }

    void EndSceneRenderToTexture()
    {
        EditorUi::EndSceneRenderToTexture();
    }

    void RenderRuntimeSceneToWindow(HWND hwnd)
    {
        if (hwnd == NULL || RuntimeStateRef().g_renderDevice == nullptr)
        {
            return;
        }
        if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        {
            return;
        }

        RECT clientRect = {};
        if (GetClientRect(hwnd, &clientRect))
        {
            const LONG clientWidth = clientRect.right - clientRect.left;
            const LONG clientHeight = clientRect.bottom - clientRect.top;
            if (clientWidth <= 0 || clientHeight <= 0)
            {
                return;
            }

            const UINT width = static_cast<UINT>((std::max)(1L, clientWidth));
            const UINT height = static_cast<UINT>((std::max)(1L, clientHeight));
            auto& cachedSize = g_viewportSizes[hwnd];
            if (cachedSize.cx != static_cast<LONG>(width) || cachedSize.cy != static_cast<LONG>(height))
            {
                if (hwnd == RuntimeStateRef().g_hwnd)
                {
                    Application::SetWindowSize(static_cast<int>(width), static_cast<int>(height));
                    EditorUi::RequestSceneRenderSize(width, height);
                }
                RuntimeStateRef().g_renderDevice->ResizeRenderTarget(hwnd, width, height);
                cachedSize.cx = static_cast<LONG>(width);
                cachedSize.cy = static_cast<LONG>(height);
            }

            Application::SetWindowSize(
                static_cast<int>(width),
                static_cast<int>(height));
        }

        const ViewportRenderMode viewportMode = ResolveViewportRenderMode(hwnd);
        RuntimeStateRef().g_renderDevice->PreRenderTarget(hwnd, RuntimeStateRef().g_gameClearColor);
        SceneManager::GetInstance().Render(viewportMode);
        RenderSpriteRenderers(viewportMode);
        RuntimeStateRef().g_renderDevice->RenderTarget(hwnd);
    }
}

void RenderSpriteRenderers(ViewportRenderMode viewportMode)
{
    RuntimeState& state = RuntimeStateRef();
    for (auto& spriteRendererEntry : state.g_spriteRenderers)
    {
        if (spriteRendererEntry.second != nullptr)
        {
            spriteRendererEntry.second->Render(state.g_renderDevice.get(), viewportMode);
        }
    }
}

void DestroyAllSpriteRenderers()
{
    RuntimeStateRef().g_spriteRenderers.clear();
    RuntimeStateRef().g_nextSpriteRendererHandle = 1;
    TextureAssetManager::Get().Clear();
}





void AppRuntime::SetGameClearColor(float r, float g, float b, float a)
{
    RuntimeStateRef().g_gameClearColor[0] = r;
    RuntimeStateRef().g_gameClearColor[1] = g;
    RuntimeStateRef().g_gameClearColor[2] = b;
    RuntimeStateRef().g_gameClearColor[3] = a;
}


///=====================================================
/// <summary>
/// 新しい SpriteRenderer を作成して内部状態に登録します。成功すると一意のハンドルを返し、失敗時は0を返します。作成時にトランスフォームを設定し、状態文字列（g_pieGameStatus）を更新します。
/// </summary>
/// <returns>作成された SpriteRenderer のハンドル（uint32_t）。作成に失敗した場合は0を返します。</returns>
///=====================================================
uint32_t AppRuntime::CreateSpriteRenderer()
{
    try
    {
        const uint32_t handle = RuntimeStateRef().g_nextSpriteRendererHandle++;
        std::unique_ptr<ISpriteRenderObject> spriteRenderer = CreateSpriteRenderObjectForBackend(RuntimeStateRef().g_rendererBackend);
        if (spriteRenderer == nullptr)
        {
            RuntimeStateRef().g_pieGameStatus = "CreateSpriteRenderer failed: unsupported backend";
            return 0;
        }

        spriteRenderer->SetTransform(0.0f, 0.0f, 0.8f, 1.4f);
        RuntimeStateRef().g_spriteRenderers[handle] = std::move(spriteRenderer);
        RuntimeStateRef().g_pieGameStatus = "SpriteRenderer created. handle=" + std::to_string(handle);
        return handle;
    }
    catch (const std::exception& ex)
    {
        RuntimeStateRef().g_pieGameStatus = std::string("CreateSpriteRenderer failed: ") + ex.what();
        return 0;
    }
    catch (...)
    {
        RuntimeStateRef().g_pieGameStatus = "CreateSpriteRenderer failed: unknown error";
        return 0;
    }
}

void AppRuntime::DestroySpriteRenderer(uint32_t handle)
{
    if (handle == 0)
    {
        return;
    }

    RuntimeStateRef().g_spriteRenderers.erase(handle);
}

void AppRuntime::SetSpriteRendererTransform(uint32_t handle, float centerX, float centerY, float width, float height)
{
    const auto it = RuntimeStateRef().g_spriteRenderers.find(handle);
    if (it == RuntimeStateRef().g_spriteRenderers.end() || it->second == nullptr)
    {
        return;
    }

    it->second->SetTransform(centerX, centerY, width, height);
}

void AppRuntime::SetSpriteRendererTexture(uint32_t handle, TextureHandle textureHandle)
{
    const auto it = RuntimeStateRef().g_spriteRenderers.find(handle);
    if (it == RuntimeStateRef().g_spriteRenderers.end() || it->second == nullptr)
    {
        return;
    }

    it->second->SetTextureHandle(textureHandle);
}

void AppRuntime::SetSpriteRendererMaterial(uint32_t handle, const char* materialName)
{
    const auto it = RuntimeStateRef().g_spriteRenderers.find(handle);
    if (it == RuntimeStateRef().g_spriteRenderers.end() || it->second == nullptr || materialName == nullptr)
    {
        return;
    }

    it->second->SetMaterialName(materialName);
}

TextureHandle AppRuntime::AcquireTextureHandle(const char* texturePath)
{
    const TextureHandle handle = TextureAssetManager::Get().AcquireTexture(texturePath);
    if (handle == 0)
    {
        if (RuntimeStateRef().g_rendererBackend != RendererBackend::DirectX12)
        {
            RuntimeStateRef().g_pieGameStatus =
                std::string("Texture skipped on ") +
                RendererBackendToString(RuntimeStateRef().g_rendererBackend) +
                ": " +
                (texturePath != nullptr ? texturePath : "(null)");
        }
        else
        {
            RuntimeStateRef().g_pieGameStatus = std::string("Texture acquire failed: ") + (texturePath != nullptr ? texturePath : "(null)");
        }
    }
    else
    {
        RuntimeStateRef().g_pieGameStatus = std::string("Texture acquired: ") + (texturePath != nullptr ? texturePath : "(null)") + " handle=" + std::to_string(handle);
    }
    return handle;
}

void AppRuntime::ReleaseTextureHandle(TextureHandle textureHandle)
{
    TextureAssetManager::Get().ReleaseTexture(textureHandle);
}

///==========================================================
/// @brief メッセージループ
///==========================================================
void AppRuntime::MessageLoopIteration()
{
    static uint32_t frameCounter = 0;
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

    ApplyPendingRendererSwitch();

    const RendererBackend activeRenderBackend =
        (RuntimeStateRef().g_renderDevice != nullptr)
        ? RuntimeStateRef().g_renderDevice->Backend()
        : RuntimeStateRef().g_displayRendererBackend;

	m_PlayInEditor.UpdatePie();

    constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    SceneManager::GetInstance().Update(kFixedDeltaTime);
    TickPieManagedAutoPublish(kFixedDeltaTime);

    if (m_PlayInEditor.IsPieRunning())
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
        if (activeRenderBackend == RendererBackend::DirectX12)
        {
            EditorUi::EnsureSceneRenderSize();
        }
    }

    const bool isNonDxBackend = (activeRenderBackend != RendererBackend::DirectX12);

    bool framePresentedExplicitly = false;

    if (activeRenderBackend == RendererBackend::DirectX12)
    {
        if (RuntimeStateRef().g_imguiInitialized)
        {
            BeginSceneRenderToTexture();
            SceneManager::GetInstance().Render(ViewportRenderMode::Scene);
            RenderSpriteRenderers(ViewportRenderMode::Scene);
            EndSceneRenderToTexture();

            if (RuntimeStateRef().g_renderDevice != nullptr)
            {
                RuntimeStateRef().g_renderDevice->PreRender(RuntimeStateRef().g_gameClearColor);
            }
        }
        else if (RuntimeStateRef().g_renderDevice != nullptr)
        {
            // Qt editor mode disables the DLL UI; render the same runtime scene into every native viewport.
            RenderRuntimeSceneToWindow(RuntimeStateRef().g_hwnd);
            RenderRuntimeSceneToWindow(RuntimeStateRef().g_gameHwnd);
            framePresentedExplicitly = true;
        }
    }
    else if (RuntimeStateRef().g_renderDevice != nullptr)
    {
        RuntimeStateRef().g_renderDevice->PreRender(RuntimeStateRef().g_gameClearColor);
    }

    if (isNonDxBackend)
    {
        SceneManager::GetInstance().Render(ViewportRenderMode::Game);
        RenderSpriteRenderers(ViewportRenderMode::Game);

        if (RuntimeStateRef().g_imguiInitialized && RuntimeStateRef().g_renderDevice != nullptr)
        {
            UINT requestedWidth = 0;
            UINT requestedHeight = 0;
            EditorUi::GetRequestedSceneRenderSize(&requestedWidth, &requestedHeight);
            RuntimeStateRef().g_renderDevice->CaptureEditorSceneTexture(requestedWidth, requestedHeight);
        }
    }

    if (RuntimeStateRef().g_imguiInitialized)
    {
        const std::string moduleSourceText = RuntimeStateRef().g_pieGameSourceModulePath.empty()
            ? (RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath.empty() ? "(none)" : RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath.u8string())
            : RuntimeStateRef().g_pieGameSourceModulePath;

        EditorUiRuntimeState uiState = {};
        uiState.isPieRunning = m_PlayInEditor.IsPieRunning();
        uiState.currentRendererBackend = static_cast<uint32_t>(RuntimeStateRef().g_rendererBackend);
        uiState.pieGameStatus = RuntimeStateRef().g_pieGameStatus.c_str();
        uiState.pieGameLastLoadError = RuntimeStateRef().g_pieGameLastLoadError.c_str();
        uiState.moduleSourceText = moduleSourceText.c_str();
        uiState.pieGameModulePath = RuntimeStateRef().g_pieGameModulePath.c_str();
        uiState.pieManagedLastPublishLogPath = RuntimeStateRef().g_pieManagedLastPublishLogPath.c_str();
        uiState.activeQuadCount = static_cast<int>(RuntimeStateRef().g_spriteRenderers.size());

        EditorUiCallbacks uiCallbacks = {};
        uiCallbacks.startPie = &StartPie;
        uiCallbacks.stopPie = &StopPie;
        uiCallbacks.setRendererBackend = &SetRendererBackendFromEditorUi;

        bool imguiRenderContextReady = true;
        if (RuntimeStateRef().g_renderDevice != nullptr && isNonDxBackend)
        {
            imguiRenderContextReady = RuntimeStateRef().g_renderDevice->PrepareImGuiRenderContext();
        }

        if (imguiRenderContextReady)
        {
            EditorUi::RenderFrame(RuntimeStateRef().g_isStandaloneMode, RuntimeStateRef().g_renderDevice.get(), uiState, uiCallbacks);
        }
        else
        {
            static uint32_t imguiContextFailureCounter = 0;
            ++imguiContextFailureCounter;
            if ((imguiContextFailureCounter % 120) == 0)
            {
                LOG_DEBUG("PrepareImGuiRenderContext failed: backend=%s",
                    RendererBackendToString(activeRenderBackend));
            }
        }
    }

    if (RuntimeStateRef().g_renderDevice != nullptr && !framePresentedExplicitly)
    {
        RuntimeStateRef().g_renderDevice->Render();
    }

    if (activeRenderBackend != RendererBackend::DirectX12 &&
        RuntimeStateRef().g_hwnd != NULL)
    {
        InvalidateRect(RuntimeStateRef().g_hwnd, nullptr, FALSE);
    }

    static uint32_t nonDxProgressCounter = 0;
    if (activeRenderBackend != RendererBackend::DirectX12)
    {
        ++nonDxProgressCounter;
        if ((nonDxProgressCounter % 120) == 0)
        {
            LOG_DEBUG("NonDX loop progress: backend=%s imgui=%d",
                RendererBackendToString(activeRenderBackend),
                RuntimeStateRef().g_imguiInitialized ? 1 : 0);
        }
    }
    else
    {
        nonDxProgressCounter = 0;
    }

    ++frameCounter;
    if ((frameCounter % 240) == 0)
    {
        LOG_DEBUG("Frame heartbeat: selected=%s active=%s imgui=%d pie=%d",
            RendererBackendToString(RuntimeStateRef().g_rendererBackend),
            RendererBackendToString(activeRenderBackend),
            RuntimeStateRef().g_imguiInitialized ? 1 : 0,
            m_PlayInEditor.IsPieRunning() ? 1 : 0);
    }
}

BOOL AppRuntime::SetRendererBackend(uint32_t backend)
{
    RendererBackend targetBackend = RendererBackend::DirectX12;

    switch (backend)
    {
    case static_cast<uint32_t>(RendererBackend::DirectX12):
        targetBackend = RendererBackend::DirectX12;
        break;
    case static_cast<uint32_t>(RendererBackend::Vulkan):
        targetBackend = RendererBackend::Vulkan;
        break;
    case static_cast<uint32_t>(RendererBackend::OpenGL):
        targetBackend = RendererBackend::OpenGL;
        break;
    default:
        return FALSE;
    }

    RuntimeStateRef().g_pendingRendererSwitch = true;
    RuntimeStateRef().g_pendingRendererBackend = targetBackend;
    RuntimeStateRef().g_pieGameStatus =
        std::string("Renderer switch requested: ") +
        RendererBackendToString(RuntimeStateRef().g_displayRendererBackend) +
        " -> " +
        RendererBackendToString(targetBackend);
    return TRUE;
}

uint32_t AppRuntime::GetRendererBackend() const
{
    return static_cast<uint32_t>(RuntimeStateRef().g_rendererBackend);
}

void AppRuntime::SetEditorUiEnabled(BOOL enabled)
{
    RuntimeStateRef().g_editorUiEnabled = (enabled != FALSE);
}

BOOL AppRuntime::IsEditorUiEnabled() const
{
    return RuntimeStateRef().g_editorUiEnabled ? TRUE : FALSE;
}

const char* AppRuntime::GetRuntimeStatusText() const
{
    return RuntimeStateRef().g_pieGameStatus.c_str();
}

const char* AppRuntime::GetRuntimeLastErrorText() const
{
    return RuntimeStateRef().g_pieGameLastLoadError.c_str();
}
