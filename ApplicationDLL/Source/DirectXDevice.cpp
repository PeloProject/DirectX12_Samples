// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include <Windows.h>
#include "DirectXDevice.h"



ID3D12Device* DirectXDevice::m_Device = nullptr;

#ifdef _DEBUG
// @brief デバッグレイヤーを有効化する関数
void DirectXDevice::EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer))))
	{
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
	else
	{
		LOG_DEBUG("Debug Layerの有効化に失敗");
	}
}
#endif

bool DirectXDevice::Initialize(HWND hwnd)
{
	LOG_DEBUG("DirectXの初期化を開始");
	EnableDebugLayer(); // デバッグレイヤーを有効化
	CreateDXGIFactory(); // DXGIファクトリの作成
	CreateDevice();      // デバイスの作成
	return true;
}

/// <summary>
/// DirectXのグラフィックインターフェースファクトリを作成する
/// GPU、ディスプレイ、ウィンドウシステム間の低レベルインターフェース提供するものとなります。
/// </summary>
/// <returns></returns>
bool DirectXDevice::CreateDXGIFactory()
{
	// DXGIファクトリの作成
#ifdef _DEBUG
	if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_DxgiFactory)) != S_OK)
	{
		LOG_DEBUG("DXGIファクトリの作成に失敗");
		return false;
	}
#else
	if (CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		LOG_DEBUG("DXGIファクトリの作成に失敗");
		return false;
	}
#endif
	return true;
}

IDXGIAdapter* DirectXDevice::GetAdapter()
{
	// アダプタの取得
	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tempAdapter = nullptr;

	for (UINT i = 0; m_DxgiFactory->EnumAdapters(i, &tempAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tempAdapter);
	}
	// アダプタの情報を取得
	for (size_t i = 0; i < adapters.size(); ++i)
	{
		DXGI_ADAPTER_DESC desc;
		adapters[i]->GetDesc(&desc);
		LOG_DEBUG("Adapter %d: %ls", i, desc.Description);

		// ここでアダプタの情報を使って何か処理を行うことができます
		if (desc.VendorId == 0x10DE) // NVIDIAのアダプタを探す
		{
			LOG_DEBUG("NVIDIA Adapter found: %ls", desc.Description);
			return adapters[i];
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

	return nullptr;
}

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
		if (D3D12CreateDevice(adapter, level, IID_PPV_ARGS(&m_Device)) == S_OK)
		{
			featureLevel = level;
			break;
		}
	}

	if (m_Device == nullptr)
	{
		//DebugOutputFormatString("DirectXの初期化に失敗\n");
		return false;
	}
#ifdef _DEBUG
	//m_Device->QueryInterface(IID_PPV_ARGS(&debugDevice));
#endif // _DEBUG
}