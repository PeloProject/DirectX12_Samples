#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

// @brief �R���\�[����ʂɃt�H�[�}�b�g�t����������o�͂���֐�
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
		PostQuitMessage(0); // OS�ɏI����ʒm
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// @brief �E�B���h�E�v���V�[�W��
#ifdef _DEBUG
int main()
{
#else
int WINAPI WINAPI WinMain(HINSTANCE hInstance, LPSTR lpCmdLine)
{

#endif
	// �E�B���h�E�N���X�̓o�^
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProcedure;
	wc.lpszClassName = L"DirectX12_Samples";
	wc.hInstance = GetModuleHandle(nullptr);
	RegisterClassEx(&wc); // �E�B���h�E�N���X�̓o�^

	RECT wrct = { 0, 0, 800, 600 }; // �E�B���h�E�̃T�C�Y

	AdjustWindowRect(&wrct, WS_OVERLAPPEDWINDOW, FALSE); // �E�B���h�E�̃T�C�Y�𒲐�

	// �E�B���h�E�̍쐬
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

	ShowWindow(hwnd, SW_SHOW); // �E�B���h�E��\��
	
	DebugOutputFormatString("Hello, World!\n");

	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) break; // WM_QUIT���b�Z�[�W��������I��
		}
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance); // �E�B���h�E�N���X�̓o�^����
	
	return 0;
}