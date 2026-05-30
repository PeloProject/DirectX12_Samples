
#include "pch.h"
#include "MeshObject.h"
#include "../Analyzer/PMDAnalyzer.h"
#include "Source/Dx12RenderDevice.h"
#include <cstring>
#include <d3d12.h>
#include "d3dx12.h" // ヘッダーをインクルード

MeshObject::MeshObject(string fileName)
{
	// PMDAnalyzerを使用して頂点バッファを作成
	PMDAnalyzer analyzer(fileName);
	const auto& vertices = analyzer.GetVertices();
	if (vertices.empty())
	{
		return;
	}

	const auto vertexBufferSize = static_cast<UINT>(sizeof(PMDVertex) * vertices.size());

	// 頂点バッファの作成
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	HRESULT hr = Dx12RenderDevice::GetDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pVertexBuffer)
	);

	if (FAILED(hr))
	{
		// エラー処理
		return;
	}

	// 頂点データをGPUに転送
	unsigned char* vertexMap = nullptr;
	hr = m_pVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
	if (SUCCEEDED(hr))
	{
		memcpy(vertexMap, vertices.data(), vertexBufferSize);
		D3D12_RANGE writtenRange = { 0, vertexBufferSize };
		m_pVertexBuffer->Unmap(0, &writtenRange);
	}
	else
	{
		return;
	}

	// 頂点バッファビューの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONE_NO",	0, DXGI_FORMAT_R16G16_UINT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHT",		0, DXGI_FORMAT_R8_UINT,			0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "EDGE_FLAG",	0, DXGI_FORMAT_R8_UINT,			0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = vertexBufferSize;
	m_VertexBufferView.StrideInBytes = sizeof(PMDVertex);
}
