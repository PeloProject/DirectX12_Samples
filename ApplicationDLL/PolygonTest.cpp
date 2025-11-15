#include "pch.h"
#include "PolygonTest.h"
#include "DirectXDevice.h"


PolygonTest::PolygonTest()
{
	m_Vertices[0] = { -1.0, -1.0, 0.0f };
	m_Vertices[1] = { -1.0,  1.0, 0.0f };
	m_Vertices[2] = {  1.0, -1.0, 0.0f };
}

void PolygonTest::CreateGpuResources()
{
	ID3D12Resource* vertexBuffer = nullptr;

	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード可能なヒープタイプ
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = sizeof(m_Vertices);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// Gpuメモリの確保
	HRESULT hr = DirectXDevice::GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	);
	if( !SUCCEEDED(hr) ) {
		return;
	}

	// 頂点データの転送
	DirectX::XMFLOAT3* vertexMap = nullptr;
	hr = vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
	if (SUCCEEDED(hr)) {
		memcpy(vertexMap, m_Vertices, sizeof(m_Vertices));
		D3D12_RANGE writtenRange = { 0, sizeof(m_Vertices) }; // 書き込み範囲
		vertexBuffer->Unmap(0, &writtenRange);
	}

	// 頂点バッファビューの設定
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(m_Vertices);		// 全バイト数
	vbView.StrideInBytes = sizeof(m_Vertices[0]);	// 1頂点あたりのサイズ


	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3DCompileFromFile(
		L"Shader/BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVs",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		&errorBlob
	);

	if( !SUCCEEDED(hr) ) {
		if( errorBlob ) {
			LOG_DEBUG("Vertex Shader Compile Error: %s", (char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		return;
	}

	ID3DBlob* psBlob = nullptr;
	hr = D3DCompileFromFile(
		L"Shader/BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVs",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		&errorBlob
	);

	if (!SUCCEEDED(hr)) {
		if (errorBlob) {
			LOG_DEBUG("Pixcel Shader Compile Error: %s", (char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		return;
	}

	D3D12_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};

}

void PolygonTest::ApplyTransformations()
{
	DirectX::XMFLOAT3* vertexMap = nullptr;

}