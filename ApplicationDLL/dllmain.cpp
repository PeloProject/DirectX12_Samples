// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include "Source/DirectXDevice.h"

//BOOL APIENTRY DllMain( HMODULE hModule,
//                       DWORD  ul_reason_for_call,
//                       LPVOID lpReserved
//                     )
//{
//    switch (ul_reason_for_call)
//    {
//    case DLL_PROCESS_ATTACH:
//    case DLL_THREAD_ATTACH:
//    case DLL_THREAD_DETACH:
//    case DLL_PROCESS_DETACH:
//        break;
//    }
//    return TRUE;
//}

#include <tchar.h>

static HWND g_hwnd = NULL;
static DirectXDevice g_DxDevice;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // 背景を塗りつぶし
        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        // テキスト描画
        const TCHAR* text = _T("Hello from C++!");
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

extern "C" __declspec(dllexport) HWND CreateNativeWindow()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    LPCTSTR className = _T("NativeWindowClass");

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hwnd = CreateWindow(
        className,
        _T("Native Window"),
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL, NULL,
        hInstance,
        NULL
    );

	if (g_hwnd != NULL)
	{
		g_DxDevice.Initialize(g_hwnd);
	}

    return g_hwnd;
}

extern "C" __declspec(dllexport) void ShowNativeWindow()
{
    if (g_hwnd != NULL)
    {
        ShowWindow(g_hwnd, SW_SHOW);
        UpdateWindow(g_hwnd);
    }
}

extern "C" __declspec(dllexport) void HideNativeWindow()
{
    if (g_hwnd != NULL)
    {
        ShowWindow(g_hwnd, SW_HIDE);
    }
}

extern "C" __declspec(dllexport) void DestroyNativeWindow()
{
    if (g_hwnd != NULL)
    {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
}

extern "C" __declspec(dllexport) void MessageLoopIteration()
{
    MSG msg = {};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}