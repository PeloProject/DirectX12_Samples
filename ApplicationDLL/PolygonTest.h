#pragma once
#include "MathUtil.h"
#include <d3d12.h>

using namespace WL;
using Microsoft::WRL::ComPtr;

class PolygonTest
{

public:
	PolygonTest();
	~PolygonTest() {}

	void Render();
private:

	void CreateVertexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc);
	void CreateIndexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc);
	void CreateGpuResources();
	void ApplyTransformations();

	/// <summary>
	/// グラフィックスパイプラインステートを作成します。
	/// GPUに描画方法を指示する為の設定情報をまとめたものになります。
	/// </summary>
	void CreateGraphicsPipelineState();

	/// <summary>
	/// シェーダーのコンパイル
	/// </summary>
	HRESULT CompileShaders();

	Vector3 m_Vertices[4];

	ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr;

	ComPtr<ID3DBlob> m_pVertexShaderBlob = nullptr;
	ComPtr<ID3DBlob> m_pPixelShaderBlob = nullptr;

	ComPtr<ID3D12RootSignature> m_pRootSignature = nullptr;

	ComPtr<ID3D12Resource> m_pVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> m_pIndexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	m_VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW		m_IndexBufferView = {};

	std::vector<short> m_Indices;
};

