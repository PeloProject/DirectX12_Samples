#include "pch.h"
#include "PolygonTest.h"
#include "Source/Dx12RenderDevice.h"
#include <algorithm>
#include <string>

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
QuadRenderObject::QuadRenderObject()
{
	ApplyQuadTransform();
	m_TextureTest.LoadFromFile(L"textest.png");

	m_TextureData.resize(256 * 256);
	for (auto& tex : m_TextureData)
	{
		tex.R = rand() % 256;
		tex.G = rand() % 256;
		tex.B = rand() % 256;
		tex.A = 255;
	}

	if (CreateMeshResources() != S_OK || InitializeMaterial() != S_OK)
	{
		throw std::runtime_error("QuadRenderObject initialization failed.");
	}
}

void QuadRenderObject::SetTransform(float centerX, float centerY, float width, float height)
{
	m_quadTransform.centerX = centerX;
	m_quadTransform.centerY = centerY;
	m_quadTransform.width = (std::max)(width, 0.01f);
	m_quadTransform.height = (std::max)(height, 0.01f);
	m_isVertexDirty = true;
}

void QuadRenderObject::SetTextureHandle(TextureHandle textureHandle)
{
	textureAsset_ = TextureAssetManager::Get().GetTexture(textureHandle);
	if (textureAsset_ == nullptr)
	{
		return;
	}

	m_material.SetTexture(textureAsset_.get());
}

void QuadRenderObject::SetMaterialName(const std::string& materialName)
{
	if (materialName.empty())
	{
		return;
	}

	// Current built-in path only supports unlit textured quads.
	if (materialName == "BuiltInMaterials::UnlitTexture")
	{
		materialName_ = materialName;
	}
}

///=========================================================================================
/// <summary>
/// m_quadTransformの中心座標と幅・高さに基づいて、四角形の4頂点の位置とUV座標をm_Verticesに設定するメンバ関数。
/// </summary>
///=========================================================================================
void QuadRenderObject::ApplyQuadTransform()
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

void QuadRenderObject::UploadVertexBufferData()
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
void QuadRenderObject::CreateVertexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc)
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

void QuadRenderObject::CreateIndexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc)
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

HRESULT QuadRenderObject::CreateMeshResources()
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

	return S_OK;
}

HRESULT QuadRenderObject::InitializeMaterial()
{
	Material::MaterialDesc materialDesc = Material::CreateBuiltInTexturedQuadDesc(&m_TextureTest);
	auto hr = m_material.Initialize(
		Dx12RenderDevice::GetDevice(),
		GetPipelineLibrary(),
		materialDesc);
	if (FAILED(hr))
	{
		return hr;
	}

	return hr;
}

void QuadRenderObject::Render()
{
	if (m_isVertexDirty)
	{
		ApplyQuadTransform();
		UploadVertexBufferData();
		m_isVertexDirty = false;
	}

	ID3D12GraphicsCommandList* commandList = Dx12RenderDevice::GetCommandList();
	if (commandList == nullptr)
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
	m_material.Bind(m_pCommandList.Get());

	m_pCommandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
	m_pCommandList->RSSetScissorRects(1, &scissorRect);
	m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	m_pCommandList->IASetIndexBuffer(&m_IndexBufferView);
	m_pCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
