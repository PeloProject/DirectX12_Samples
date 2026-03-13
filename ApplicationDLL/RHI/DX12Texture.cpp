#include "pch.h"
#include "DX12Texture.h"
#include "DescriptorHeapManager.h"
#include "Source/Dx12RenderDevice.h"

DX12Texture::DX12Texture()
{
	// Textureの読み込み処理
	DirectX::ScratchImage scratchImage = {};
	HRESULT hr = DirectX::LoadFromWICFile(L"C:/Users/shinji/Documents/Projects/DirectX12_Samples/Assets/Texture/textest.png", DirectX::WIC_FLAGS_NONE, &m_Metadata, scratchImage);
	auto image = scratchImage.GetImage(0, 0, 0);//生データ抽出

	// ここでm_pImageTextureBufferにテクスチャデータを転送する処理を実装
	D3D12_HEAP_PROPERTIES texHeapProps = {};
	texHeapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; //ライトバックで
	texHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; //L0プール
	texHeapProps.CreationNodeMask = 0;
	texHeapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC texResourceDesc = {};
	texResourceDesc.Format = m_Metadata.format;
	texResourceDesc.Width = static_cast<UINT64>(m_Metadata.width);
	texResourceDesc.Height = static_cast<UINT>(m_Metadata.height);
	texResourceDesc.DepthOrArraySize = static_cast<UINT16>(m_Metadata.arraySize);
	texResourceDesc.SampleDesc.Count = 1;
	texResourceDesc.SampleDesc.Quality = 0;
	texResourceDesc.MipLevels = static_cast<UINT16>(m_Metadata.mipLevels);
	texResourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_Metadata.dimension);
	texResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = Dx12RenderDevice::GetDevice()->CreateCommittedResource(
		&texHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pTextureBuffer)
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("LoadTexture: CreateCommittedResource failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return;
	}

	// テクスチャデータの転送
	hr = m_pTextureBuffer->WriteToSubresource(
		0, // DstSubresource
		nullptr, // pDstBox
		image->pixels, // pSrcData
		static_cast<UINT>(image->rowPitch), // SrcRowPitch
		static_cast<UINT>(image->slicePitch) // SrcDepthPitch
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("LoadTexture: WriteToSubresource failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return;
	}
}


void DX12Texture::LoadTexture()
{

}