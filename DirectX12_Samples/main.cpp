#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl/client.h>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;
using Microsoft::WRL::ComPtr;
// DirectXの初期化に必要なグローバル変数
ID3D12Device* device = nullptr;
IDXGIFactory6* dxgiFactory = nullptr;
IDXGISwapChain4* swapChain = nullptr;
ID3D12GraphicsCommandList* commandList = nullptr;
ID3D12CommandAllocator* commandAllocator = nullptr;
ID3D12DescriptorHeap* rtvHeap = nullptr;
ID3D12CommandQueue* commandQueue = nullptr;
ID3D12Fence* fence = nullptr;
UINT64 fenceValue = 0; // フェンスの値
ComPtr<ID3D12DebugDevice> debugDevice;

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

// @brief デバッグレイヤーを有効化する関数
void EnableDebugLayer()
{
#ifdef _DEBUG
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer))))
	{
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
	else
	{
		DebugOutputFormatString("Debug Layerの有効化に失敗\n");
	}
#endif
}

// @brief DirectXの初期化処理
bool InitializeDirectX(HWND hwnd)
{
	DebugOutputFormatString("DirectXの初期化を開始\n");

	EnableDebugLayer(); // デバッグレイヤーを有効化

	// DXGIファクトリの作成
#ifdef _DEBUG
	if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		DebugOutputFormatString("DXGIファクトリの作成に失敗\n");
		return false;
	}
#else
	if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		DebugOutputFormatString("DXGIファクトリの作成に失敗\n");
		return false;
	}
#endif
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
#ifdef _DEBUG
	device->QueryInterface(IID_PPV_ARGS(&debugDevice));
#endif // _DEBUG


	DebugOutputFormatString("DirectXの初期化が正常に終了\n");

	//-----
	// コマンドリスト


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

	// レンダーターターゲットビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 2; // バックバッファの数
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // フラグはなし
	rtvHeapDesc.NodeMask = 0; // ノードマスクは0（単一アダプター使用時）

	if (device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)) != S_OK)
	{
		DebugOutputFormatString("レンダーターゲットビューのヒープの作成に失敗\n");
		return false;
	}

	// スワップチェインの取得
	DXGI_SWAP_CHAIN_DESC swapChainDesc2;
	if (swapChain->GetDesc(&swapChainDesc2) != S_OK)
	{
		DebugOutputFormatString("スワップチェインの取得に失敗\n");
		return false;
	}

	// バックバッファの取得
	vector<ID3D12Resource*> backBuffers(swapChainDesc2.BufferCount);
	for (UINT i = 0; i < swapChainDesc2.BufferCount; ++i)
	{
		if (swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])) != S_OK)
		{
			DebugOutputFormatString("バックバッファの取得に失敗\n");
			return false;
		}
	}

	// レンダーターゲットビューの作成
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < swapChainDesc2.BufferCount; ++i)
	{
		device->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle);
		rtvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// フェンスの作成
	if (device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)) != S_OK)
	{
		DebugOutputFormatString("フェンスの作成に失敗\n");
		return false;
	}


	return true;
}

void UpdateDirectX(HWND hwnd)
{
	// DirectXの更新処理をここに記述
	// 例えば、レンダリングやリソースの更新など

	// コマンドリストのリセット
	commandAllocator->Reset();

	auto bbidx = swapChain->GetCurrentBackBufferIndex(); // 現在のバックバッファのインデックスを取得

	auto rtvH = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbidx * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	commandList->OMSetRenderTargets(1, &rtvH, true, nullptr); // レンダーターゲットを設定

	// 画面をクリア
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // 黒色でクリア
	commandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	commandList->Close(); // コマンドリストを閉じる

	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists); // コマンドリストを実行

	commandQueue->Signal(fence, ++fenceValue); // フェンスにシグナルを送る
	if (fence->GetCompletedValue() != fenceValue)
	{
		HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr); // イベントハンドルを作成
		fence->SetEventOnCompletion(fenceValue, eventHandle); // フェンスの完了イベントを設定
		WaitForSingleObject(eventHandle, INFINITE); // イベントが完了するまで待機
		CloseHandle(eventHandle); // イベントハンドルを閉じる
	}

	commandAllocator->Reset(); // コマンドアロケータをリセット
	commandList->Reset(commandAllocator, nullptr); // コマンドリストをリセット

	swapChain->Present(1, 0); // スワップチェインをプレゼント（画面に表示）
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

		UpdateDirectX(hwnd); // DirectXの更新処理
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance); // ウィンドウクラスの登録解除

	debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
	debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_IGNORE_INTERNAL);
	
	return 0;
}