#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

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
// @brief DirectXの初期化処理
bool InitializeDirectX(HWND hwnd)
{
	DebugOutputFormatString("DirectXの初期化を開始\n");
	ID3D12Device* device = nullptr;
	IDXGIFactory6* dxgiFactory = nullptr;
	IDXGISwapChain4* swapChain = nullptr;

	// DXGIファクトリの作成
	if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		DebugOutputFormatString("DXGIファクトリの作成に失敗\n");
		return false;
	}
	// アダプタの取得
	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tempAdapter = nullptr;

	for (UINT i = 0; dxgiFactory->EnumAdapters(i, &tempAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tempAdapter);
	}
	// アダプタの情報を取得
	for (size_t i = 0; i < adapters.size(); ++i)
	{
		DXGI_ADAPTER_DESC desc;
		adapters[i]->GetDesc(&desc);
		DebugOutputFormatString("Adapter %d: %ls\n", i, desc.Description);

		// ここでアダプタの情報を使って何か処理を行うことができます
		if (desc.VendorId == 0x10DE) // NVIDIAのアダプタを探す
		{
			DebugOutputFormatString("NVIDIA Adapter found: %ls\n", desc.Description);
			tempAdapter = adapters[i];
			break;
		}
		else if (desc.VendorId == 0x1002) // AMDのアダプタを探す
		{
			DebugOutputFormatString("AMD Adapter found: %ls\n", desc.Description);
		}
		else if (desc.VendorId == 0x8086) // Intelのアダプタを探す
		{
			DebugOutputFormatString("Intel Adapter found: %ls\n", desc.Description);
		}
	}


	// Deviceの作成
	D3D_FEATURE_LEVEL levels[] = 
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	D3D_FEATURE_LEVEL featureLevel;

	for (auto level : levels)
	{
		if (D3D12CreateDevice(tempAdapter, level, IID_PPV_ARGS(&device)) == S_OK)
		{
			featureLevel = level;
			break;
		}
	}

	if (device == nullptr)
	{
		DebugOutputFormatString("DirectXの初期化に失敗\n");
		return false;
	}

	DebugOutputFormatString("DirectXの初期化が正常に終了\n");

	//-----
	// コマンドリスト
	ID3D12CommandList* commandList = nullptr;
	ID3D12CommandAllocator* commandAllocator = nullptr;

	if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)) != S_OK)
	{
		DebugOutputFormatString("コマンドアロケータの作成に失敗\n");
		return false;
	}
	
	if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList)) != S_OK)
	{
		DebugOutputFormatString("コマンドリストの作成に失敗\n");
		return false;
	}

	// コマンドリストの実行
	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	// コマンドリストと合わせます。
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// タイムアウトなし
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// プライオリティの指定は無し
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	// アダプターを１つしか使用しない時は０でよい
	queueDesc.NodeMask = 0;
	if (device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)) != S_OK)
	{
		DebugOutputFormatString("コマンドキューの作成に失敗\n");
		return false;
	}

	// スワップチェインの作成
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = 800; // ウィンドウの幅
	swapChainDesc.Height = 600; // ウィンドウの高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // バッファのフォーマット
	swapChainDesc.Stereo = false; // ステレオ表示はしない
	
	
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプリングの数
	swapChainDesc.SampleDesc.Quality = 0; // マルチサンプリングの品質
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER; // バッファの使用法
	swapChainDesc.BufferCount = 2; // バッファ数

	
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // バックバッファは伸び縮み可能
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は速やかに破棄
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // アルファモードは指定しない
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // ウィンドウ、フルスクリーンモード変更を許可

	if (dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&swapChain) != S_OK)
	{
		DebugOutputFormatString("スワップチェインの作成に失敗\n");
		return false;
	}

	return true;
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
	
	InitializeDirectX(hwnd); // DirectXの初期化

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