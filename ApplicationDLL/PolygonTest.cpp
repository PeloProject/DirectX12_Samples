#include "pch.h"
#include "PolygonTest.h"
#include "Source/Dx12RenderDevice.h"
#include <algorithm>

namespace
{
PipelineLibrary& GetPipelineLibrary()
{
	static PipelineLibrary library;
	return library;
}
}

//=========================================================================================
// 
// @brief コンストラクタ
// 
//=========================================================================================
PolygonTest::PolygonTest()
{
	ApplyQuadTransform();

	m_TextureData.resize(256 * 256);
	for (auto& tex : m_TextureData)
	{
		tex.R = rand() % 256;
		tex.G = rand() % 256;
		tex.B = rand() % 256;
		tex.A = 255;
	}

	if (CreateGpuResources() != S_OK)
	{
		throw std::runtime_error("CreateGpuResources is failed.");
	}
}

void PolygonTest::SetTransform(float centerX, float centerY, float width, float height)
{
	m_quadTransform.centerX = centerX;
	m_quadTransform.centerY = centerY;
	m_quadTransform.width = (std::max)(width, 0.01f);
	m_quadTransform.height = (std::max)(height, 0.01f);
	m_isVertexDirty = true;
}

///=========================================================================================
/// <summary>
/// m_quadTransformの中心座標と幅・高さに基づいて、四角形の4頂点の位置とUV座標をm_Verticesに設定するメンバ関数。
/// </summary>
///=========================================================================================
void PolygonTest::ApplyQuadTransform()
{
	const float halfWidth = m_quadTransform.width * 0.5f;
	const float halfHeight = m_quadTransform.height * 0.5f;
	const float left = m_quadTransform.centerX - halfWidth;
	const float right = m_quadTransform.centerX + halfWidth;
	const float bottom = m_quadTransform.centerY - halfHeight;
	const float top = m_quadTransform.centerY + halfHeight;

	m_Vertices[0] = { { left, bottom, 0.0f }, { 0.0f, 1.0f } };
	m_Vertices[1] = { { left, top, 0.0f }, { 0.0f, 0.0f } };
	m_Vertices[2] = { { right, bottom, 0.0f }, { 1.0f, 1.0f } };
	m_Vertices[3] = { { right, top, 0.0f }, { 1.0f, 0.0f } };
}

void PolygonTest::UploadVertexBufferData()
{
	if (m_pVertexBuffer == nullptr)
	{
		return;
	}

	Vertex* vertexMap = nullptr;
	if (SUCCEEDED(m_pVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap))))
	{
		memcpy(vertexMap, m_Vertices, sizeof(m_Vertices));
		D3D12_RANGE writtenRange = { 0, sizeof(m_Vertices) };
		m_pVertexBuffer->Unmap(0, &writtenRange);
	}
}

///=========================================================================================
/// <summary>
/// 頂点バッファ用のコミット済みGPUリソースを作成し、クラス内の頂点データを転送して頂点バッファビューを設定します。
/// </summary>
/// <param name="heapProps">D3D12_HEAP_PROPERTIES 構造体。頂点バッファのためのヒープ割り当てのプロパティ（メモリ種類や作成方法など）を指定します。</param>
/// <param name="resourceDesc">D3D12_RESOURCE_DESC 構造体。作成するリソースのサイズ、フォーマット、使用方法などの記述を指定します。</param>
///==========================================================================================
void PolygonTest::CreateVertexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc)
{
	HRESULT hr = Dx12RenderDevice::GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pVertexBuffer)
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("CreateVertexBuffer: CreateCommittedResource failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return;
	}

	Vertex* vertexMap = nullptr;
	hr = m_pVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
	if (SUCCEEDED(hr)) {
		memcpy(vertexMap, m_Vertices, sizeof(m_Vertices));
		D3D12_RANGE writtenRange = { 0, sizeof(m_Vertices) };
		m_pVertexBuffer->Unmap(0, &writtenRange);
	}
	else
	{
		LOG_DEBUG("CreateVertexBuffer: Map failed. hr=0x%08X", static_cast<unsigned int>(hr));
	}

	m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = sizeof(m_Vertices);
	m_VertexBufferView.StrideInBytes = sizeof(m_Vertices[0]);
}

void PolygonTest::CreateIndexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc)
{
	HRESULT hr = Dx12RenderDevice::GetDeviceComPtr()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pIndexBuffer)
	);

	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("CreateIndexBuffer: CreateCommittedResource failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return;
	}

	unsigned short indices[] = {
	0, 1, 2,
	2, 1, 3
	};

	m_Indices = std::vector<short>(std::begin(indices), std::end(indices));

	unsigned short* indexMap = nullptr;
	hr = m_pIndexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("CreateIndexBuffer: Map failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return;
	}

	std::copy(std::begin(m_Indices), std::end(m_Indices), indexMap);
	m_pIndexBuffer->Unmap(0, nullptr);

	m_IndexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_IndexBufferView.SizeInBytes = static_cast<UINT>(sizeof(m_Indices[0]) * m_Indices.size());
}

HRESULT PolygonTest::CreateGpuResources()
{
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
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

	CreateVertexBuffer(heapProps, resourceDesc);
	CreateIndexBuffer(heapProps, resourceDesc);
	if (m_pVertexBuffer == nullptr || m_pIndexBuffer == nullptr)
	{
		return E_FAIL;
	}

	HRESULT hr = CreateTextureBuffer();
	if (FAILED(hr))
	{
		return hr;
	}

	static const D3D12_INPUT_ELEMENT_DESC kInputLayout[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	PipelineLibrary::PipelineKey pipelineKey = {};
	pipelineKey.vertexShaderFile = L"BasicVertexShader.hlsl";
	pipelineKey.vertexEntryPoint = "BasicVS";
	pipelineKey.vertexShaderModel = "vs_5_0";
	pipelineKey.pixelShaderFile = L"BasicPixelShader.hlsl";
	pipelineKey.pixelEntryPoint = "BasicPS";
	pipelineKey.pixelShaderModel = "ps_5_0";
	pipelineKey.renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineKey.cullMode = D3D12_CULL_MODE_NONE;
	pipelineKey.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineKey.enableDepth = false;
	pipelineKey.enableBlend = false;

	hr = GetPipelineLibrary().GetOrCreate(
		Dx12RenderDevice::GetDevice(),
		pipelineKey,
		kInputLayout,
		_countof(kInputLayout),
		&m_pipeline);
	if (FAILED(hr))
	{
		return hr;
	}

	return hr;
}

HRESULT PolygonTest::CreateTextureBuffer()
{
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapProps.CreationNodeMask = 0;
	heapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.Width = 256;
	resourceDesc.Height = 256;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = Dx12RenderDevice::GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_pTextureBuffer)
	);

	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("Texture Buffer Create Error");
		return hr;
	}

	hr = m_pTextureBuffer->WriteToSubresource(
		0,
		nullptr,
		m_TextureData.data(),
		static_cast<UINT>(256 * sizeof(TextureRGBA)),
		static_cast<UINT>(m_TextureData.size() * sizeof(TextureRGBA))
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("Texture Data Write Error");
		return hr;
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	descriptorHeapDesc.NumDescriptors = 1;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	hr = Dx12RenderDevice::GetDevice()->CreateDescriptorHeap(
		&descriptorHeapDesc,
		IID_PPV_ARGS(&m_pTexDescHeap)
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	Dx12RenderDevice::GetDevice()->CreateShaderResourceView(
		m_pTextureBuffer.Get(),
		&srvDesc,
		m_pTexDescHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return hr;
}

void PolygonTest::ApplyTransformations()
{
	DirectX::XMFLOAT3* vertexMap = nullptr;
	(void)vertexMap;
}

void PolygonTest::Render()
{
	if (m_isVertexDirty)
	{
		ApplyQuadTransform();
		UploadVertexBufferData();
		m_isVertexDirty = false;
	}

	ID3D12GraphicsCommandList* commandList = Dx12RenderDevice::GetCommandList();
	if (commandList == nullptr || m_pipeline == nullptr)
	{
		return;
	}

	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<FLOAT>(Application::GetWindowWidth());
	viewport.Height = static_cast<FLOAT>(Application::GetWindowHeight());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	ComPtr<ID3D12GraphicsCommandList> m_pCommandList = commandList;
	m_pCommandList->SetPipelineState(m_pipeline->pipelineState.Get());
	m_pCommandList->SetGraphicsRootSignature(m_pipeline->rootSignature.Get());
	m_pCommandList->SetDescriptorHeaps(1, m_pTexDescHeap.GetAddressOf());
	m_pCommandList->SetGraphicsRootDescriptorTable(0, m_pTexDescHeap->GetGPUDescriptorHandleForHeapStart());

	m_pCommandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
	m_pCommandList->RSSetScissorRects(1, &scissorRect);
	m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	m_pCommandList->IASetIndexBuffer(&m_IndexBufferView);
	m_pCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
