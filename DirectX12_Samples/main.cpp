#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

// @brief コンソール画面にフォーマット付き文字列を出力する関数
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0); // OSに終了を通知
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// @brief ウィンドウプロシージャ
#ifdef _DEBUG
int main()
{
#else
int WINAPI WINAPI WinMain(HINSTANCE hInstance, LPSTR lpCmdLine)
{

#endif
	// ウィンドウクラスの登録
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProcedure;
	wc.lpszClassName = L"DirectX12_Samples";
	wc.hInstance = GetModuleHandle(nullptr);
	RegisterClassEx(&wc); // ウィンドウクラスの登録

	RECT wrct = { 0, 0, 800, 600 }; // ウィンドウのサイズ

	AdjustWindowRect(&wrct, WS_OVERLAPPEDWINDOW, FALSE); // ウィンドウのサイズを調整

	// ウィンドウの作成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"DirectX12 Samples",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wrct.right - wrct.left,
		wrct.bottom - wrct.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	ShowWindow(hwnd, SW_SHOW); // ウィンドウを表示
	
	DebugOutputFormatString("Hello, World!\n");

	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) break; // WM_QUITメッセージが来たら終了
		}
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance); // ウィンドウクラスの登録解除
	
	return 0;
}