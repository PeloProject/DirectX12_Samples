#pragma once

#include "GraphicsDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>


using namespace std;
using Microsoft::WRL::ComPtr;

class DirectXDevice :  public GraphicsDevice
{
public:
	virtual ~DirectXDevice();

	bool Initialize(HWND hwnd, UINT width, UINT height);

	void EnableDebugLayer();

	static ID3D12Device* GetDevice() { return m_pDevice.Get(); }
	static ID3D12GraphicsCommandList* GetCommandList() { return m_pCommandList.Get(); }


	/// <summary>
	/// DirectXのグラフィックインターフェースファクトリを作成する。
	/// 一番初めに作成する必要があります。
	/// GPU、ディスプレイ、ウィンドウシステム間の低レベルインターフェース提供するものとなります。
	/// </summary>
	/// <returns>生成が成功したか</returns>
	bool CreateDXGIFactory();

	IDXGIAdapter* GetAdapter();

	bool CreateDevice();

	bool CreateCommandList();

	bool CreateCommandQueue();

	bool CreateSwapChain(HWND hwnd, UINT width, UINT height);

	bool CreateRenderTargetView();

	bool CreateFence();

	void Render();
	void PreRender();

	void WaitForPreviousFrame();

	void Shutdown();  // ★追加

private:

#ifdef _DEBUG
	void ReportLiveDeviceObjects(ComPtr<ID3D12DebugDevice>& debugDevice);
#endif

	static ComPtr<ID3D12Device> m_pDevice; // 初期化はcppで行う
	ComPtr<IDXGIFactory6> m_pDxgiFactory = nullptr;
	ComPtr<IDXGISwapChain4> m_pSwapChain = nullptr;
	static ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	ComPtr<ID3D12CommandAllocator> m_pCommandAllocator = nullptr;
	ComPtr<ID3D12CommandQueue> m_pCommandQueue = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_pRenderTargetViewHeap = nullptr;
	ComPtr<ID3D12Fence> m_pFence = nullptr;
	ComPtr<IDXGIAdapter> m_pAdapters = nullptr;
	UINT64 m_FenceValue = 0; // フェンスの値
	vector<ComPtr<ID3D12Resource>> m_pBackBuffers;

	bool m_isShutdown = false;  // ★追加
};
