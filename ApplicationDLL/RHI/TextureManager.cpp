#include "pch.h"
#include "TextureManager.h"
#include "DX12Texture.h"
#include "DescriptorHeapManager.h"
#include "Source/Dx12RenderDevice.h"

/// シングルトンインスタンスの取得
TextureManager& TextureManager::Get()
{
	static TextureManager instance;
	return instance;
}

/// <summary>
/// テクスチャーリソースを作成して初期化
/// </summary>
UINT TextureManager::CreateTextureResource(ComPtr<ID3D12Resource>& textureBuffer, const wchar_t* filePath)
{
	// Textureの読み込み処理
	DirectX::ScratchImage scratchImage = {};
	DirectX::TexMetadata metadata = {};
	HRESULT hr = DirectX::LoadFromWICFile(filePath, DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
    if (FAILED(hr))
    {
        LOG_DEBUG("Failed to load texture from file: %ls. hr=0x%08X", filePath, static_cast<unsigned int>(hr));
        return -1;
	}
	auto image = scratchImage.GetImage(0, 0, 0);//生データ抽出

	// ここでm_pImageTextureBufferにテクスチャデータを転送する処理を実装
	D3D12_HEAP_PROPERTIES texHeapProps = {};
	texHeapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; //ライトバックで
	texHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; //L0プール
	texHeapProps.CreationNodeMask = 0;
	texHeapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC texResourceDesc = {};
	texResourceDesc.Format = metadata.format;
	texResourceDesc.Width = static_cast<UINT64>(metadata.width);
	texResourceDesc.Height = static_cast<UINT>(metadata.height);
	texResourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	texResourceDesc.SampleDesc.Count = 1;
	texResourceDesc.SampleDesc.Quality = 0;
	texResourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	texResourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	texResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = Dx12RenderDevice::GetDevice()->CreateCommittedResource(
		&texHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureBuffer)
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("LoadTexture: CreateCommittedResource failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return -1;
	}

	// テクスチャデータの転送
	hr = textureBuffer->WriteToSubresource(
		0, // DstSubresource
		nullptr, // pDstBox
		image->pixels, // pSrcData
		static_cast<UINT>(image->rowPitch), // SrcRowPitch
		static_cast<UINT>(image->slicePitch) // SrcDepthPitch
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("LoadTexture: WriteToSubresource failed. hr=0x%08X", static_cast<unsigned int>(hr));
		return -1;
	}

	
	// テクスチャリソースの作成と初期化
	UINT handleIndex = DescriptorHeapManager::Get().AllocateGlobalTextureDescriptor();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = metadata.mipLevels;

	Dx12RenderDevice::GetDevice()->CreateShaderResourceView(
		textureBuffer.Get(),
		&srvDesc,
		DescriptorHeapManager::Get().GetCPUHandle(handleIndex));

	return handleIndex;
}

/// <summary>
/// テクスチャリソースの作成と初期化
/// </summary>
UINT TextureManager::CreateShaderResoureView()
{
 //   auto textureHeap = DescriptorHeapManager::Get().GetGlobalTextureHeap();
 //   DX12Texture* dx12Texture = static_cast<DX12Texture*>(desc.textureResource);
 //  
	//// テクスチャリソースの作成と初期化
 //   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
 //   srvDesc.Format = dx12Texture->GetMetadata().format;
 //   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
 //   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
 //   srvDesc.Texture2D.MipLevels = dx12Texture->GetMetadata().mipLevels;
	//
 //   Dx12RenderDevice::GetDevice()->CreateShaderResourceView(
 //       static_cast<ID3D12Resource*>(desc.textureResource->GetTextureBuffer()),
 //       &srvDesc,
 //       textureHeap->GetCPUDescriptorHandleForHeapStart());
	return 0;
}