#include "pch.h"
#include "Material.h"

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

    hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(textureHeap_.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.textureFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = desc.textureMipLevels;

    device->CreateShaderResourceView(
        desc.textureResource,
        &srvDesc,
        textureHeap_->GetCPUDescriptorHandleForHeapStart());

    return S_OK;
}

void Material::Bind(ID3D12GraphicsCommandList* commandList) const
{
    if (commandList == nullptr || pipeline_ == nullptr || textureHeap_ == nullptr)
    {
        return;
    }

    commandList->SetPipelineState(pipeline_->pipelineState.Get());
    commandList->SetGraphicsRootSignature(pipeline_->rootSignature.Get());
    commandList->SetDescriptorHeaps(1, textureHeap_.GetAddressOf());
    commandList->SetGraphicsRootDescriptorTable(0, textureHeap_->GetGPUDescriptorHandleForHeapStart());
}
