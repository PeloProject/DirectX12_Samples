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
    bool InitializeImGui()
    {
        return EditorUi::Initialize(
            RuntimeStateRef().g_rendererBackend,
            RuntimeStateRef().g_hwnd,
            RuntimeStateRef().g_renderDevice != nullptr ? RuntimeStateRef().g_renderDevice->GetDx12CommandQueue() : nullptr,
            static_cast<UINT>(Application::GetWindowWidth()),
            static_cast<UINT>(Application::GetWindowHeight()));
    }

    void ShutdownImGui()
    {
        EditorUi::Shutdown();
        RuntimeStateRef().g_imguiInitialized = false;
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
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        EditorUi::Shutdown();
        RuntimeStateRef().g_imguiInitialized = false;
        if (RuntimeStateRef().g_rendererBackend == RendererBackend::DirectX12)
        {
            if (RuntimeStateRef().g_renderDevice != nullptr)
            {
                RuntimeStateRef().g_renderDevice->Shutdown();
            }
        }
        RuntimeStateRef().g_renderDevice.reset();
        RuntimeStateRef().g_hwnd = NULL;
        RuntimeStateRef().g_rendererBackendLocked = false;
        return 0;
    default:
        if (RuntimeStateRef().g_imguiInitialized && EditorUi::HandleWndProc(hwnd, msg, wParam, lParam))
        {
            return 1;
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
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    RuntimeStateRef().g_rendererBackend = ResolveRendererBackendFromEnvironment(RuntimeStateRef().g_rendererBackend);
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

        RuntimeStateRef().g_renderDevice = CreateRenderDevice(RuntimeStateRef().g_rendererBackend);
        if (RuntimeStateRef().g_renderDevice == nullptr)
        {
            RuntimeStateRef().g_imguiInitialized = false;
            RuntimeStateRef().g_pieGameStatus = "Failed to create render device";
        }
        else if (!RuntimeStateRef().g_renderDevice->Initialize(RuntimeStateRef().g_hwnd, Application::GetWindowWidth(), Application::GetWindowHeight()))
        {
            RuntimeStateRef().g_imguiInitialized = false;
            RuntimeStateRef().g_pieGameStatus = "Render device initialization failed";
            RuntimeStateRef().g_renderDevice.reset();
        }
        else if (RuntimeStateRef().g_rendererBackend == RendererBackend::DirectX12)
        {
            ConfigureD3D12DebugFilters();
            RuntimeStateRef().g_imguiInitialized = InitializeImGui();
        }
        else
        {
            RuntimeStateRef().g_imguiInitialized = InitializeImGui();
            RuntimeStateRef().g_pieGameStatus =
                std::string("Renderer backend selected: ") + RendererBackendToString(RuntimeStateRef().g_rendererBackend) +
                " (scene texture path is DX12-only)";
        }

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
    if (RuntimeStateRef().g_renderDevice != nullptr)
    {
        RuntimeStateRef().g_renderDevice->Shutdown();
    }
    RuntimeStateRef().g_renderDevice.reset();
    DestroyWindow(RuntimeStateRef().g_hwnd);
    RuntimeStateRef().g_hwnd = NULL;
    RuntimeStateRef().g_rendererBackendLocked = false;
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
        Application::SetWindowSize(static_cast<int>(width), static_cast<int>(height));
        EditorUi::RequestSceneRenderSize(width, height);
        if (RuntimeStateRef().g_renderDevice != nullptr)
        {
            RuntimeStateRef().g_renderDevice->Resize(width, height);
        }
        if (RuntimeStateRef().g_imguiInitialized)
        {
            EditorUi::EnsureSceneRenderSize();
        }
    }

}
