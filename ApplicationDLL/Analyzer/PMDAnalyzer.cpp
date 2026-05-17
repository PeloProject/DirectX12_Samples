
#include "pch.h"
#include "PMDAnalyzer.h"
#include "Source/Dx12RenderDevice.h"
#include <d3d12.h>
#include "d3dx12.h" // ヘッダーをインクルード

PMDAnalyzer::PMDAnalyzer(string fileName)
{
	PMDHeader header;
	char signature[3];
	FILE* fp;
	auto err = fopen_s(&fp, fileName.c_str(), "rb");
	if (err != 0) {
		// エラー処理
		return;
	}

	// headerの読み込み
	fread(signature, sizeof(signature), 1, fp);
	fread(&header, sizeof(header), 1, fp);

	// データの読み込みや解析をここで行う
	unsigned int vertexCount;
	fread(&vertexCount, sizeof(vertexCount), 1, fp);

	std::vector<unsigned char> vertices(vertexCount * sizeof(PMDVertex));
	fread(vertices.data(), vertices.size(), 1, fp);

	// 頂点バッファの作成
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
	HRESULT hr =Dx12RenderDevice::GetDevice()->CreateCommittedResource(
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
	std::copy(vertices.begin(), vertices.end(), vertexMap);
	m_pVertexBuffer->Unmap(0, nullptr);

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


	fclose(fp);
}