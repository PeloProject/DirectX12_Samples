#pragma once
#include "MathUtil.h"
#include "Source/PipelineLibrary.h"
#include <d3d12.h>

#include <memory>

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
	void SetTransform(float centerX, float centerY, float width, float height);
private:

	void CreateVertexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc);
	void CreateIndexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc);
	HRESULT CreateGpuResources();
	HRESULT CreateTextureBuffer();

	void ApplyTransformations();
	void ApplyQuadTransform();
	void UploadVertexBufferData();

	struct QuadTransform
	{
		float centerX = 0.0f;
		float centerY = 0.0f;
		float width = 0.8f;
		float height = 1.4f;
	};

	QuadTransform m_quadTransform = {};
	Vertex m_Vertices[4];
	bool m_isVertexDirty = true;

	std::shared_ptr<const PipelineLibrary::Pipeline> m_pipeline;

	ComPtr<ID3D12Resource>		m_pVertexBuffer;
	ComPtr<ID3D12Resource>		m_pIndexBuffer;
	ComPtr<ID3D12Resource>		m_pTextureBuffer;
	D3D12_VERTEX_BUFFER_VIEW	m_VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW		m_IndexBufferView = {};

	std::vector<TextureRGBA> m_TextureData;
	ComPtr<ID3D12DescriptorHeap> m_pTexDescHeap;

	std::vector<short> m_Indices;
};
