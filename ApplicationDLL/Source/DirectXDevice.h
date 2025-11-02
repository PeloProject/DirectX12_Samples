#pragma once

#include "GraphicsDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>
class DirectXDevice :  public GraphicsDevice
{
public:
	virtual ~DirectXDevice() {}

	bool Initialize(HWND hwnd);

	void EnableDebugLayer();

	static ID3D12Device* GetDevice() { return m_Device; }

	/// <summary>
	/// DirectXのグラフィックインターフェースファクトリを作成する。
	/// 一番初めに作成する必要があります。
	/// GPU、ディスプレイ、ウィンドウシステム間の低レベルインターフェース提供するものとなります。
	/// </summary>
	/// <returns>生成が成功したか</returns>
	bool CreateDXGIFactory();

	IDXGIAdapter* GetAdapter();

	bool CreateDevice();

private:
	static ID3D12Device* m_Device;
	IDXGIFactory6* m_DxgiFactory = nullptr;
};

