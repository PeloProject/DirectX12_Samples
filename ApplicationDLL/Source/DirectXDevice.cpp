// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include <Windows.h>
#include <cstring>
#include "DirectXDevice.h"
#ifdef _DEBUG
//#define GRAPHICS_DEBUG_MODE
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// 静的メンバー変数の初期化
ComPtr<ID3D12Device> DirectXDevice::m_pDevice;
ComPtr<ID3D12GraphicsCommandList> DirectXDevice::m_pCommandList;

#ifdef _DEBUG
// @brief デバッグレイヤーを有効化する関数
void DirectXDevice::EnableDebugLayer()
{
	const bool enableDebugLayer = []() -> bool
	{
		// Default ON in debug builds. Set DX12_DEBUG_LAYER=0 to disable.
		char* value = nullptr;
		size_t len = 0;
		const errno_t err = _dupenv_s(&value, &len, "DX12_DEBUG_LAYER");
		const bool enabled = !(err == 0 && value != nullptr && strcmp(value, "0") == 0);
		if (value != nullptr)
		{
			free(value);
		}
		return enabled;
	}();

	if (!enableDebugLayer)
	{
		return;
	}

	const bool enableGpuValidation = []() -> bool
	{
		// Default OFF: GPU-based validation is extremely slow.
		// Enable only when explicitly requested:
		//   set DX12_GPU_VALIDATION=1
		char* value = nullptr;
		size_t len = 0;
		const errno_t err = _dupenv_s(&value, &len, "DX12_GPU_VALIDATION");
		const bool enabled = (err == 0 && value != nullptr && strcmp(value, "1") == 0);
		if (value != nullptr)
		{
			free(value);
		}
		return enabled;
	}();

	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();

		// より詳細なデバッグ情報
		ComPtr<ID3D12Debug1> debugController1;
		if (SUCCEEDED(debugController.As(&debugController1))) {
			debugController1->SetEnableGPUBasedValidation(enableGpuValidation ? TRUE : FALSE);
			debugController1->SetEnableSynchronizedCommandQueueValidation(enableGpuValidation ? TRUE : FALSE);
		}

		// 情報メッセージも表示
		ComPtr<ID3D12Debug3> debugController3;
		if (SUCCEEDED(debugController.As(&debugController3))) {
			debugController3->SetEnableGPUBasedValidation(enableGpuValidation ? TRUE : FALSE);
		}
	}

	// DXGIデバッグも有効化
	ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)))) {
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
	}
}
#endif

/// <summary>
/// 終了処理
/// </summary>
void DirectXDevice::Shutdown()
{
	if (m_isShutdown) {
		LOG_DEBUG("Already shutdown\n");
		return;
	}
	m_isShutdown = true;

	WaitForPreviousFrame();

	// ComPtrですが、明示的にResetを呼んで解放しメモリリークのチェックをしやすくします
	m_pSwapChain.Reset();
	m_pCommandList.Reset();
	m_pCommandAllocator.Reset();
	m_pCommandQueue.Reset();
	m_pRenderTargetViewHeap.Reset();
	m_pFence.Reset();
	m_pAdapters.Reset();

	for (auto& buffer : m_pBackBuffers) {
		buffer.Reset();
	}
	m_pBackBuffers.clear();
#ifdef _DEBUG
	// デバイスを解放する前にデバッグインターフェースを取得します。
	ComPtr<ID3D12DebugDevice> debugDevice;
	if (m_pDevice) {
		m_pDevice.As(&debugDevice);
	}
#endif
	m_pDevice.Reset();
	m_pDxgiFactory.Reset();

#ifdef _DEBUG
	// ライブオブジェクトのレポート
	if (debugDevice)
	{
		ReportLiveDeviceObjects(debugDevice);
	}
#endif
}

/// <summary>
/// デストラクタ
/// </summary>
DirectXDevice::~DirectXDevice()
{
	// デストラクタでも念のため Shutdown を呼ぶ（二重呼び出し防止付き）
	if (!m_isShutdown) {
		Shutdown();
	}
}

#ifdef _DEBUG
/// <summary>
/// ライブオブジェクトのレポート
/// </summary>
/// <param name="debugDevice"></param>
void DirectXDevice::ReportLiveDeviceObjects(ComPtr<ID3D12DebugDevice>& debugDevice)
{
	// D3D12デバイスのライブオブジェクト
	if (!debugDevice) {
		LOG_DEBUG("D3D12 debug device is unavailable. Skipping live object report.");
		return;
	}

	LOG_DEBUG("=== D3D12 Debug Report START ===");
	LOG_DEBUG("=== デバッグ用のライブオブジェクト表示の為、DeviceのRefは1あります。 ===");
	debugDevice->ReportLiveDeviceObjects(
		D3D12_RLDO_SUMMARY |
		D3D12_RLDO_DETAIL |
		D3D12_RLDO_IGNORE_INTERNAL
	);
	debugDevice.Reset();
	LOG_DEBUG("=== D3D12 Debug Report END ===");

	// DXGIのライブオブジェクト
	LOG_DEBUG("Attempting DXGI debug report...");
	ComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
		LOG_DEBUG("=== DXGI Debug Report START ===");
		dxgiDebug->ReportLiveObjects(
			DXGI_DEBUG_ALL,
			DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)
		);
		LOG_DEBUG("=== DXGI Debug Report END ===");
	}
	else {
		LOG_DEBUG("ERROR: Failed to get IDXGIDebug1");
	}

	LOG_DEBUG("ReportLiveDeviceObjects: Exit");
}
#endif

bool DirectXDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
	LOG_DEBUG("DirectXの初期化を開始");
	m_isShutdown = false;

#ifdef _DEBUG
	EnableDebugLayer(); // デバッグレイヤーを有効化
#endif
	if (!CreateGraphicsInterface()) { return false; } // DXGIファクトリの作成
	if (!CreateDevice()) { return false; }            // デバイスの作成
	if (!CreateCommandList()) { return false; }       // コマンドリストの作成
	if (!CreateCommandQueue()) { return false; }      // コマンドキューの作成
	if (!CreateSwapChain(hwnd, width, height)) { return false; } // スワップチェインの作成
	if (!CreateRenderTargetView()) { return false; }  // レンダーターゲットビューの作成
	if (!CreateFence()) { return false; }             // フェンスの作成
	return true;
}

///======================================================================================================
/// <summary>
/// DirectXのグラフィックインターフェースファクトリを作成する
/// GPU、ディスプレイ、ウィンドウシステム間の低レベルインターフェース提供するものとなります。
/// </summary>
/// <returns></returns>
///======================================================================================================
bool DirectXDevice::CreateGraphicsInterface()
{
	// DXGIファクトリの作成
#ifdef _DEBUG

	//HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pDxgiFactory));
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pDxgiFactory));

	if (FAILED(hr))
	{
		char errorMsg[256];
		sprintf_s(errorMsg, "CreateDXGIFactory2 failed with HRESULT: 0x%08X\n", hr);
		OutputDebugStringA(errorMsg);

		// 具体的なエラーコードによる分岐
		switch (hr)
		{
		case E_INVALIDARG:
			OutputDebugStringA("Error: E_INVALIDARG - Invalid argument\n");
			break;
		case E_OUTOFMEMORY:
			OutputDebugStringA("Error: E_OUTOFMEMORY\n");
			break;
		case DXGI_ERROR_NOT_FOUND:
			OutputDebugStringA("Error: DXGI_ERROR_NOT_FOUND\n");
			break;
		case DXGI_ERROR_UNSUPPORTED:
			OutputDebugStringA("Error: DXGI_ERROR_UNSUPPORTED - Debug layer not supported\n");
			break;
		default:
			OutputDebugStringA("Error: Unknown error\n");
			break;
		}
	}
#else
	if (CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pDxgiFactory)) != S_OK)
	{
		LOG_DEBUG("DXGIファクトリの作成に失敗");
		return false;
	}
#endif
	return true;
}

/// <summary>
/// DirectXのアダプタを取得
/// </summary>
/// <returns></returns>
IDXGIAdapter* DirectXDevice::GetAdapter()
{
	// アダプタの取得（GPU優先で取得）
	for (UINT i = 0; ; ++i)
	{
		ComPtr<IDXGIAdapter> tempAdapter;
		if (m_pDxgiFactory->EnumAdapters(i, &tempAdapter) == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}
		DXGI_ADAPTER_DESC desc;
		tempAdapter->GetDesc(&desc);
		LOG_DEBUG("Adapter %d: %ls", i, desc.Description);

		// ここでアダプタの情報を使って何か処理を行うことができます
		if (desc.VendorId == 0x10DE) // NVIDIAのアダプタを探す
		{
			LOG_DEBUG("NVIDIA Adapter found: %ls", desc.Description);
			m_pAdapters = tempAdapter;
			break;
		}
		else if (desc.VendorId == 0x1002) // AMDのアダプタを探す
		{
			LOG_DEBUG("AMD Adapter found: %ls", desc.Description);
		}
		else if (desc.VendorId == 0x8086) // Intelのアダプタを探す
		{
			LOG_DEBUG("Intel Adapter found: %ls", desc.Description);
		}
	}

	return m_pAdapters.Get();
}

/// <summary>
/// DirectXデバイスの作成
/// </summary>
/// <returns></returns>
bool DirectXDevice::CreateDevice()
{
	IDXGIAdapter* adapter = GetAdapter();

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
		if (D3D12CreateDevice(adapter, level, IID_PPV_ARGS(m_pDevice.GetAddressOf())) == S_OK)
		{
			featureLevel = level;
			break;
		}
	}

	if (m_pDevice == nullptr)
	{
		LOG_DEBUG("DirectXの初期化に失敗\n");
		return false;
	}
#ifdef _DEBUG
	//m_Device->QueryInterface(IID_PPV_ARGS(&debugDevice));
#endif // _DEBUG
	return true;
}

/// <summary>
/// コマンドリストの作成
/// </summary>
/// <returns></returns>
bool DirectXDevice::CreateCommandList()
{
	if (m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_pCommandAllocator.GetAddressOf())) != S_OK)
	{
		LOG_DEBUG("コマンドアロケータの作成に失敗\n");
		return false;
	}

	if (m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pCommandList)) != S_OK)
	{
		LOG_DEBUG("コマンドリストの作成に失敗\n");
		return false;
	}

	return true;
}

bool DirectXDevice::CreateCommandQueue()
{
	// コマンドリストの実行 これは後々必要になるのでここで作成しておく

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	// コマンドリストと合わせます。
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// タイムアウトなし
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// プライオリティの指定は無し
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	// アダプターを１つしか使用しない時は０でよい
	queueDesc.NodeMask = 0;
	if (m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue)) != S_OK)
	{
		LOG_DEBUG("コマンドキューの作成に失敗\n");
		return false;
	}

	return true;
}

bool DirectXDevice::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
	// スワップチェインの作成
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = width; // ウィンドウの幅
	swapChainDesc.Height = height; // ウィンドウの高さ
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

	if (m_pDxgiFactory->CreateSwapChainForHwnd(
		m_pCommandQueue.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)m_pSwapChain.GetAddressOf()) != S_OK)
	{
		LOG_DEBUG("スワップチェインの作成に失敗\n");
		return false;
	}

	return true;
}

bool DirectXDevice::CreateRenderTargetView()
{
	// レンダーターターゲットビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 2; // バックバッファの数
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // フラグはなし
	rtvHeapDesc.NodeMask = 0; // ノードマスクは0（単一アダプター使用時）

	if (m_pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pRenderTargetViewHeap)) != S_OK)
	{
		LOG_DEBUG("レンダーターゲットビューのヒープの作成に失敗\n");
		return false;
	}

	// スワップチェインの取得
	DXGI_SWAP_CHAIN_DESC swapChainDesc2;
	if (m_pSwapChain->GetDesc(&swapChainDesc2) != S_OK)
	{
		LOG_DEBUG("スワップチェインの取得に失敗\n");
		return false;
	}

	// バックバッファの取得
	m_pBackBuffers.resize(swapChainDesc2.BufferCount);
	for (UINT i = 0; i < swapChainDesc2.BufferCount; ++i)
	{
		if (m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i])) != S_OK)
		{
			LOG_DEBUG("バックバッファの取得に失敗\n");
			return false;
		}
	}

	// レンダーターゲットビューの作成
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < swapChainDesc2.BufferCount; ++i)
	{
		m_pDevice->CreateRenderTargetView(m_pBackBuffers[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return true;
}

bool DirectXDevice::Resize(UINT width, UINT height)
{
	if (m_pSwapChain == nullptr || width == 0 || height == 0)
	{
		return false;
	}

	WaitForPreviousFrame();

	for (auto& buffer : m_pBackBuffers)
	{
		buffer.Reset();
	}
	m_pBackBuffers.clear();
	m_pRenderTargetViewHeap.Reset();

	HRESULT hr = m_pSwapChain->ResizeBuffers(
		0,
		width,
		height,
		DXGI_FORMAT_UNKNOWN,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	if (FAILED(hr))
	{
		LOG_DEBUG("ResizeBuffers failed: 0x%08X", hr);
		return false;
	}

	m_BarrierDesc.Transition.pResource = nullptr;
	return CreateRenderTargetView();
}

bool DirectXDevice::CreateFence()
{
	// フェンスの作成
	if (m_pDevice->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)) != S_OK)
	{
		LOG_DEBUG("フェンスの作成に失敗\n");
		return false;
	}
	return true;
}

/// <summary>
/// DirectXのレンダリング前処理
/// </summary>
void DirectXDevice::PreRender(const float clearColor[4])
{
	if (m_pSwapChain == nullptr || m_pBackBuffers.empty())
	{
		return;
	}

	auto bbidx = m_pSwapChain->GetCurrentBackBufferIndex(); // 現在のバックバッファのインデックスを取得

	// バックバッファを描画可能状態に変更
	m_BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	m_BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	m_BarrierDesc.Transition.pResource = m_pBackBuffers[bbidx].Get(); // バックバッファのリソースを設定
	m_BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // もともとの状態
	m_BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 変更後の状態
	m_pCommandList->ResourceBarrier(1, &m_BarrierDesc);

	// レンダーターゲットの設定
	auto rtvH = m_pRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbidx * m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_pCommandList->OMSetRenderTargets(1, &rtvH, true, nullptr); // レンダーターゲットを設定

	// 画面をクリア
    const float defaultClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float* finalClearColor = (clearColor != nullptr) ? clearColor : defaultClearColor;
    m_pCommandList->ClearRenderTargetView(rtvH, finalClearColor, 0, nullptr);

}

/// <summary>
/// レンダリング処理
/// </summary>
void DirectXDevice::Render()
{
	if (m_pSwapChain == nullptr || m_pBackBuffers.empty())
	{
		return;
	}

	// DirectXの更新処理をここに記述
	// 例えば、レンダリングやリソースの更新など

	// バックバッファをプレゼント可能状態に変更
	m_BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // もともとの状態
	m_BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // 変更後の状態
	m_pCommandList->ResourceBarrier(1, &m_BarrierDesc);

	m_pCommandList->Close(); // コマンドリストを閉じる

	ID3D12CommandList* commandLists[] = { m_pCommandList.Get()};
	m_pCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists); // コマンドリストを実行

	WaitForPreviousFrame();

	m_pCommandAllocator->Reset(); // コマンドアロケータをリセット
	m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr); // コマンドリストをリセット

	m_pSwapChain->Present(1, 0); // スワップチェインをプレゼント（画面に表示）
}

/// <summary>
/// 前のフレーム（GPU 上のコマンド実行）が完了するまで待機します。コマンドキューにフェンスをシグナルし、
/// フェンスの完了値が到達していない場合はイベントを作成して待機し、完了後にイベントハンドルを閉じます。
/// イベント作成に失敗した場合はログ出力などのエラー処理が行われます。
/// </summary>
void DirectXDevice::WaitForPreviousFrame()
{
	if (m_pCommandQueue == nullptr || m_pFence == nullptr)
	{
		return;
	}

	// 前のフレームが完了するまで待機
	const HRESULT signalHr = m_pCommandQueue->Signal(m_pFence.Get(), ++m_FenceValue);
	if (FAILED(signalHr))
	{
		LOG_DEBUG("Fence signal failed: 0x%08X", signalHr);
		return;
	}
	if (m_pFence->GetCompletedValue() < m_FenceValue)
	{
		HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (eventHandle != nullptr)
		{
			const HRESULT setEventHr = m_pFence->SetEventOnCompletion(m_FenceValue, eventHandle);
			if (SUCCEEDED(setEventHr))
			{
				const DWORD waitResult = WaitForSingleObject(eventHandle, 1000);
				if (waitResult == WAIT_TIMEOUT)
				{
					LOG_DEBUG("Fence wait timeout. Continue shutdown path.");
				}
			}
			else
			{
				LOG_DEBUG("SetEventOnCompletion failed: 0x%08X", setEventHr);
			}
			CloseHandle(eventHandle);
		}
		else
		{
			// エラー処理（ログ出力や例外など、プロジェクトの方針に合わせて記述）
			LOG_DEBUG("イベントハンドルの作成に失敗しました");
		}
	}
}
