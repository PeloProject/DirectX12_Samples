#pragma once
#include "MathUtil.h"
#include <d3d12.h>

using namespace WL;
using Microsoft::WRL::ComPtr;

/// <summary>
/// RGBAテクスチャデータの構造体
/// </summary>
struct TextureRGBA
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};

/// <summary>
/// 頂点データの構造体
/// </summary>
struct Vertex
{
	Vector3 pos;
	Vector2 uv;
};

class PolygonTest
{

public:
	PolygonTest();
	~PolygonTest() {}

	void Render();
private:

	void CreateVertexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc);
	void CreateIndexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc);
	HRESULT CreateGpuResources();
	HRESULT CreateTextureBuffer();

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

	Vertex m_Vertices[4];

	ComPtr<ID3D12PipelineState> m_pPipelineState;

	ComPtr<ID3DBlob> m_pVertexShaderBlob;
	ComPtr<ID3DBlob> m_pPixelShaderBlob;

	ComPtr<ID3D12RootSignature> m_pRootSignature;

	ComPtr<ID3D12Resource>		m_pVertexBuffer;
	ComPtr<ID3D12Resource>		m_pIndexBuffer;
	ComPtr<ID3D12Resource>		m_pTextureBuffer;
	D3D12_VERTEX_BUFFER_VIEW	m_VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW		m_IndexBufferView = {};

	std::vector<TextureRGBA> m_TextureData;

	std::vector<short> m_Indices;
};

