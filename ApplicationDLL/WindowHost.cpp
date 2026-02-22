#include "pch.h"

#include "AppRuntime.h"
#include "FrameLoop.h"

#include "SceneManager.h"
#include "Source/DirectXDevice.h"
#include "Source/EditorUi.h"
#include "Source/RenderDeviceFactory.h"
#include "Source/RendererBackend.h"

#include <algorithm>
#include <tchar.h>

namespace
{
    void ConfigureD3D12DebugFilters();

    bool InitializeImGui()
    {
        return EditorUi::Initialize(
            RuntimeStateRef().g_displayRendererBackend,
            RuntimeStateRef().g_hwnd,
            RuntimeStateRef().g_renderDevice != nullptr ? RuntimeStateRef().g_renderDevice->GetDx12CommandQueue() : nullptr,
            static_cast<UINT>(Application::GetWindowWidth()),
            static_cast<UINT>(Application::GetWindowHeight()));
    }

    void ShutdownRendererAndUi()
    {
        EditorUi::Shutdown();
        RuntimeStateRef().g_imguiInitialized = false;

        if (RuntimeStateRef().g_renderDevice != nullptr)
        {
            RuntimeStateRef().g_renderDevice->Shutdown();
            RuntimeStateRef().g_renderDevice.reset();
        }
    }

    bool InitializeRendererAndUi(RendererBackend backend)
    {
        RuntimeStateRef().g_displayRendererBackend = backend;
        RuntimeStateRef().g_renderDevice = CreateRenderDevice(backend);
        if (RuntimeStateRef().g_renderDevice == nullptr)
        {
            RuntimeStateRef().g_imguiInitialized = false;
            RuntimeStateRef().g_pieGameStatus = "Failed to create render device";
            return false;
        }

        if (!RuntimeStateRef().g_renderDevice->Initialize(RuntimeStateRef().g_hwnd, Application::GetWindowWidth(), Application::GetWindowHeight()))
        {
            RuntimeStateRef().g_imguiInitialized = false;
            RuntimeStateRef().g_pieGameStatus = "Render device initialization failed";
            RuntimeStateRef().g_renderDevice.reset();
            return false;
        }

        if (backend == RendererBackend::DirectX12)
        {
            ConfigureD3D12DebugFilters();
        }

        RuntimeStateRef().g_imguiInitialized = InitializeImGui();
        if (!RuntimeStateRef().g_imguiInitialized)
        {
            RuntimeStateRef().g_pieGameStatus = "Editor UI initialization failed";
            ShutdownRendererAndUi();
            return false;
        }

        RuntimeStateRef().g_pieGameStatus =
            std::string("Renderer active: ") + RendererBackendToString(RuntimeStateRef().g_rendererBackend) +
            " (UI: " + RendererBackendToString(RuntimeStateRef().g_displayRendererBackend) + ")";
        return true;
    }

    void ShutdownImGui()
    {
        EditorUi::Shutdown();
        RuntimeStateRef().g_imguiInitialized = false;
    }

    bool RecreateWindowForRendererSwitch(RendererBackend backend)
    {
        HWND oldHwnd = RuntimeStateRef().g_hwnd;
        if (oldHwnd == NULL)
        {
            return false;
        }

        RECT windowRect = {};
        const bool hasWindowRect = GetWindowRect(oldHwnd, &windowRect) != FALSE;
        const bool wasVisible = IsWindowVisible(oldHwnd) != FALSE;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        LPCTSTR className = _T("NativeWindowClass");
        HWND newHwnd = CreateWindow(
            className,
            RuntimeStateRef().g_isStandaloneMode ? _T("PieGameManaged Player") : _T("Native Window"),
            WS_OVERLAPPEDWINDOW,
            hasWindowRect ? windowRect.left : CW_USEDEFAULT,
            hasWindowRect ? windowRect.top : CW_USEDEFAULT,
            hasWindowRect ? (windowRect.right - windowRect.left) : CW_USEDEFAULT,
            hasWindowRect ? (windowRect.bottom - windowRect.top) : CW_USEDEFAULT,
            NULL, NULL,
            hInstance,
            NULL);

        if (newHwnd == NULL)
        {
            return false;
        }

        RuntimeStateRef().g_hwnd = newHwnd;
        RuntimeStateRef().g_rendererBackendLocked = true;

        RECT clientRect = {};
        if (GetClientRect(newHwnd, &clientRect))
        {
            Application::SetWindowSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            EditorUi::RequestSceneRenderSize(
                static_cast<UINT>((std::max)(1L, clientRect.right - clientRect.left)),
                static_cast<UINT>((std::max)(1L, clientRect.bottom - clientRect.top)));
        }

        const bool initialized = InitializeRendererAndUi(backend);
        if (!initialized)
        {
            DestroyWindow(newHwnd);
            RuntimeStateRef().g_hwnd = oldHwnd;
            RuntimeStateRef().g_rendererBackendLocked = true;
            return false;
        }

        if (wasVisible)
        {
            ShowWindow(newHwnd, SW_SHOW);
            UpdateWindow(newHwnd);
        }
        SetFocus(newHwnd);
        SetForegroundWindow(newHwnd);

        ShowWindow(oldHwnd, SW_HIDE);
        DestroyWindow(oldHwnd);
        return true;
    }

    void ConfigureD3D12DebugFilters()
    {
        ID3D12Device* device = DirectXDevice::GetDevice();
        if (device == nullptr)
        {
            return;
        }

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        {
            return;
        }

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
        };

        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        infoQueue->AddStorageFilterEntries(&filter);
    }
}

///=====================================================================
/// @brief ウィンドウプロシージャ
/// @param hwnd ウィンドウへのハンドル
/// @param msg  メッセージコード
/// @param wParam メッセージに関する追加パラメータ
/// @param lParam メッセージに関する追加パラメータ
/// @return 
///=====================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Runtime().HandleWndProc(hwnd, msg, wParam, lParam);
}

///=====================================================================
/// @brief ウィンドウプロシージャの操作
/// @param hwnd ウィンドウへのハンドル
/// @param msg  メッセージコード
/// @param wParam メッセージに関する追加パラメータ
/// @param lParam メッセージに関する追加パラメータ
/// @return 
///=====================================================================
LRESULT AppRuntime::HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE: ChangeWindowSize(hwnd, msg, wParam, lParam); return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps = {};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (hwnd == RuntimeStateRef().g_hwnd)
        {
            ShutdownRendererAndUi();
            RuntimeStateRef().g_hwnd = NULL;
            RuntimeStateRef().g_rendererBackendLocked = false;
        }
        return 0;
    default:
        if (RuntimeStateRef().g_imguiInitialized)
        {
            const bool handledByImGui = EditorUi::HandleWndProc(hwnd, msg, wParam, lParam);
            const bool isMouseMessage =
                msg == WM_MOUSEMOVE ||
                msg == WM_NCMOUSEMOVE ||
                msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
                msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
                msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
                msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL;
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
            {
                const int mouseX = static_cast<short>(LOWORD(lParam));
                const int mouseY = static_cast<short>(HIWORD(lParam));
                LOG_DEBUG("Mouse msg=%u handled=%d wantCapture=%d pos=(%d,%d) backend=%s",
                    msg,
                    handledByImGui ? 1 : 0,
                    EditorUi::WantsMouseCapture() ? 1 : 0,
                    mouseX,
                    mouseY,
                    RendererBackendToString(RuntimeStateRef().g_rendererBackend));
            }
            if (handledByImGui || (isMouseMessage && EditorUi::WantsMouseCapture()))
            {
                return 1;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

///=====================================================================
/// @brief ウィンドウの生成
/// @return ウィンドウハンドル
///=====================================================================
HWND AppRuntime::CreateNativeWindow()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    LPCTSTR className = _T("NativeWindowClass");

    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    UnregisterClass(className, hInstance);
    if (RegisterClass(&wc) == 0)
    {
        const DWORD registerError = GetLastError();
        LOG_DEBUG("RegisterClass failed: %lu", registerError);
        return NULL;
    }

    RuntimeStateRef().g_rendererBackend = ResolveRendererBackendFromEnvironment(RuntimeStateRef().g_rendererBackend);
    //RuntimeStateRef().g_displayRendererBackend = RendererBackend::OpenGL;
    RuntimeStateRef().g_rendererBackendLocked = true;

    RECT windowRect = { 0, 0, Application::GetWindowWidth(), Application::GetWindowHeight() };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    RuntimeStateRef().g_hwnd = CreateWindow(
        className,
        RuntimeStateRef().g_isStandaloneMode ? _T("PieGameManaged Player") : _T("Native Window"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL, NULL,
        hInstance,
        NULL);

    if (RuntimeStateRef().g_hwnd != NULL)
    {
        RECT clientRect = {};
        if (GetClientRect(RuntimeStateRef().g_hwnd, &clientRect))
        {
            Application::SetWindowSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            EditorUi::RequestSceneRenderSize(
                static_cast<UINT>((std::max)(1L, clientRect.right - clientRect.left)),
                static_cast<UINT>((std::max)(1L, clientRect.bottom - clientRect.top)));
        }

        InitializeRendererAndUi(RuntimeStateRef().g_displayRendererBackend);

        SceneManager::GetInstance().ChangeScene(0);
    }
    OutputDebugStringA("=== Main Loop START ===\n");
    return RuntimeStateRef().g_hwnd;
}

///=====================================================================
/// @brief ウィンドウの表示
///=====================================================================
void AppRuntime::ShowNativeWindow()
{
    if (RuntimeStateRef().g_hwnd == NULL)
    {
        return;
    }
    ShowWindow(RuntimeStateRef().g_hwnd, SW_SHOW);
    UpdateWindow(RuntimeStateRef().g_hwnd);
    
}

///=====================================================================
/// @brief ウィンドウの非表示
///=====================================================================
void AppRuntime::HideNativeWindow()
{
    if (RuntimeStateRef().g_hwnd == NULL)
    {
        return;
    }
    ShowWindow(RuntimeStateRef().g_hwnd, SW_HIDE);
}

///=====================================================================
/// @brief ウィンドウの削除
///=====================================================================
void AppRuntime::DestroyNativeWindow()
{
    if (RuntimeStateRef().g_hwnd == NULL)
    {
        return;
    }

    StopPieImmediate();
    ShutdownImGui();
    ShutdownRendererAndUi();
    DestroyWindow(RuntimeStateRef().g_hwnd);
    RuntimeStateRef().g_hwnd = NULL;
    RuntimeStateRef().g_rendererBackendLocked = false;
}

bool AppRuntime::ApplyPendingRendererSwitch()
{
    if (!RuntimeStateRef().g_pendingRendererSwitch || RuntimeStateRef().g_hwnd == NULL)
    {
        return false;
    }

    const bool isMouseButtonDown =
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    if (isMouseButtonDown)
    {
        return false;
    }

    RuntimeStateRef().g_pendingRendererSwitch = false;
    const RendererBackend targetBackend = RuntimeStateRef().g_pendingRendererBackend;
    const RendererBackend previousBackend = RuntimeStateRef().g_rendererBackend;
    const bool wasPieRunning = RuntimeStateRef().g_isPieRunning;
    LOG_DEBUG("ApplyPendingRendererSwitch: %s -> %s",
        RendererBackendToString(previousBackend),
        RendererBackendToString(targetBackend));

    if (targetBackend == previousBackend)
    {
        return true;
    }

    StopPieImmediate();
    LOG_DEBUG("ApplyPendingRendererSwitch: StopPieImmediate done");
    ShutdownRendererAndUi();
    LOG_DEBUG("ApplyPendingRendererSwitch: ShutdownRendererAndUi done");
    const bool needsWindowRecreate = false;
    const bool initializeOk = needsWindowRecreate
        ? RecreateWindowForRendererSwitch(targetBackend)
        : InitializeRendererAndUi(targetBackend);

    if (!initializeOk)
    {
        const std::string failedStatus =
            std::string("Renderer switch failed: ") + RendererBackendToString(targetBackend) +
            ". Rolling back to " + RendererBackendToString(previousBackend);

        ShutdownRendererAndUi();
        const bool rollbackOk = needsWindowRecreate
            ? RecreateWindowForRendererSwitch(previousBackend)
            : InitializeRendererAndUi(previousBackend);
        if (!rollbackOk)
        {
            RuntimeStateRef().g_pieGameStatus = failedStatus + " (rollback failed)";
            LOG_DEBUG("ApplyPendingRendererSwitch: rollback failed");
            return false;
        }

        RuntimeStateRef().g_pieGameStatus = failedStatus;
        LOG_DEBUG("ApplyPendingRendererSwitch: rollback success");
        EditorUi::ResetInputState();
        ReleaseCapture();
        SetFocus(RuntimeStateRef().g_hwnd);
        SetForegroundWindow(RuntimeStateRef().g_hwnd);
        if (wasPieRunning)
        {
            StartPieImmediate();
        }
        return false;
    }

    RuntimeStateRef().g_pieGameStatus =
        std::string("Renderer switched to ") + RendererBackendToString(targetBackend);
    EditorUi::ResetInputState();
    ReleaseCapture();
    SetFocus(RuntimeStateRef().g_hwnd);
    SetForegroundWindow(RuntimeStateRef().g_hwnd);
    if (wasPieRunning)
    {
        StartPieImmediate();
    }
    LOG_DEBUG("ApplyPendingRendererSwitch: success");
    return true;
}

///=====================================================================
/// @brief ウィンドウのサイズの変更
///=====================================================================
void AppRuntime::ChangeWindowSize(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hwnd != RuntimeStateRef().g_hwnd)
    {
        return;
    }
    const UINT width = LOWORD(lParam);
    const UINT height = HIWORD(lParam);
    if (wParam != SIZE_MINIMIZED && width > 0 && height > 0)
    {
        LOG_DEBUG("ChangeWindowSize: backend=%s width=%u height=%u",
            RendererBackendToString(RuntimeStateRef().g_rendererBackend), width, height);
        Application::SetWindowSize(static_cast<int>(width), static_cast<int>(height));
        EditorUi::RequestSceneRenderSize(width, height);
        if (RuntimeStateRef().g_renderDevice != nullptr)
        {
            const bool resized = RuntimeStateRef().g_renderDevice->Resize(width, height);
            LOG_DEBUG("ChangeWindowSize: renderDevice->Resize result=%d", resized ? 1 : 0);
        }
        if (RuntimeStateRef().g_imguiInitialized)
        {
            EditorUi::EnsureSceneRenderSize();
        }
    }

}
