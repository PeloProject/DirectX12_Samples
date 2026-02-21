#include "pch.h"

#include "AppRuntime.h"
#include "FrameLoop.h"

#include "SceneManager.h"
#include "Source/EditorUi.h"

#include <algorithm>
#include <tchar.h>

namespace
{
    bool InitializeImGui()
    {
        return EditorUi::Initialize(
            RuntimeStateRef().g_hwnd,
            RuntimeStateRef().g_DxDevice.GetCommandQueue(),
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Runtime().HandleWndProc(hwnd, msg, wParam, lParam);
}

LRESULT AppRuntime::HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        if (hwnd == RuntimeStateRef().g_hwnd)
        {
            const UINT width = LOWORD(lParam);
            const UINT height = HIWORD(lParam);
            if (wParam != SIZE_MINIMIZED && width > 0 && height > 0)
            {
                Application::SetWindowSize(static_cast<int>(width), static_cast<int>(height));
                EditorUi::RequestSceneRenderSize(width, height);
                RuntimeStateRef().g_DxDevice.Resize(width, height);
                if (RuntimeStateRef().g_imguiInitialized)
                {
                    EditorUi::EnsureSceneRenderSize();
                }
            }
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        EditorUi::Shutdown();
        RuntimeStateRef().g_imguiInitialized = false;
        RuntimeStateRef().g_DxDevice.Shutdown();
        RuntimeStateRef().g_hwnd = NULL;
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
        const TCHAR* text = _T("Hello from C++!");
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        if (RuntimeStateRef().g_imguiInitialized && EditorUi::HandleWndProc(hwnd, msg, wParam, lParam))
        {
            return 1;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

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

        RuntimeStateRef().g_DxDevice.Initialize(RuntimeStateRef().g_hwnd, Application::GetWindowWidth(), Application::GetWindowHeight());
        ConfigureD3D12DebugFilters();
        RuntimeStateRef().g_imguiInitialized = InitializeImGui();

        SceneManager::GetInstance().ChangeScene(0);
    }
    OutputDebugStringA("=== Main Loop START ===\n");
    return RuntimeStateRef().g_hwnd;
}

void AppRuntime::ShowNativeWindow()
{
    if (RuntimeStateRef().g_hwnd != NULL)
    {
        ShowWindow(RuntimeStateRef().g_hwnd, SW_SHOW);
        UpdateWindow(RuntimeStateRef().g_hwnd);
    }
}

void AppRuntime::HideNativeWindow()
{
    if (RuntimeStateRef().g_hwnd != NULL)
    {
        ShowWindow(RuntimeStateRef().g_hwnd, SW_HIDE);
    }
}

void AppRuntime::DestroyNativeWindow()
{
    if (RuntimeStateRef().g_hwnd != NULL)
    {
        StopPieImmediate();
        ShutdownImGui();
        RuntimeStateRef().g_DxDevice.Shutdown();
        DestroyWindow(RuntimeStateRef().g_hwnd);
        RuntimeStateRef().g_hwnd = NULL;
    }
}
