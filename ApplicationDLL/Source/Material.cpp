#include "pch.h"
#include "DescriptorHeapManager.h"
#include "Material.h"
#include "DX12Texture.h"

HRESULT Material::Initialize(
    ID3D12Device* device,
    PipelineLibrary& pipelineLibrary,
    const MaterialDesc& desc)
{
    if (device == nullptr || desc.textureResource == nullptr || desc.inputElements.empty())
    {
        return E_INVALIDARG;
    }

    HRESULT hr = pipelineLibrary.GetOrCreate(
        device,
        desc.pipelineKey,
        desc.inputElements.data(),
        static_cast<UINT>(desc.inputElements.size()),
        &pipeline_);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;

    hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(DescriptorHeapManager::Get().GetGlobalTextureHeapAddress()));
    if (FAILED(hr))
    {
        return hr;
    }

    auto textureHeap = DescriptorHeapManager::Get().GetGlobalTextureHeap();
	DX12Texture* dx12Texture = static_cast<DX12Texture*>(desc.textureResource);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = dx12Texture->GetMetadata().format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = dx12Texture->GetMetadata().mipLevels;

	
    auto handle = DescriptorHeapManager::Get().GetSRVDescriptorHandleForGlobalTextureHeap();
    device->CreateShaderResourceView(
        static_cast<ID3D12Resource*>(desc.textureResource->GetTextureBuffer()),
        &srvDesc,
        textureHeap->GetCPUDescriptorHandleForHeapStart());

    return S_OK;
}

void Material::Bind(ID3D12GraphicsCommandList* commandList) const
{

    if (commandList == nullptr || pipeline_ == nullptr)
    {
        return;
    }

    auto textureHeap = DescriptorHeapManager::Get().GetGlobalTextureHeapAddress();
	auto handle = DescriptorHeapManager::Get().GetSRVDescriptorHandleForGlobalTextureHeap();
    commandList->SetPipelineState(pipeline_->pipelineState.Get());
    commandList->SetGraphicsRootSignature(pipeline_->rootSignature.Get());
    commandList->SetDescriptorHeaps(1, textureHeap);
    commandList->SetGraphicsRootDescriptorTable(0, handle);
}
