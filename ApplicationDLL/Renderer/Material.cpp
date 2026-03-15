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


	// テクスチャの取得
	m_pTexture = desc.textureResource;

    return S_OK;
}

/// <summary>
/// マテリアルを指定したコマンドリストにバインドします。
/// commandList および内部の pipeline_ が有効な場合に、パイプラインステートとルートシグネチャを設定し、
/// グローバルなテクスチャ用ディスクリプタヒープと SRV ディスクリプタテーブルをコマンドリストにセットします。
/// commandList または pipeline_ が nullptr の場合は何もしません。
/// </summary>
/// <param name="commandList">レンダリングコマンドを記録するための
/// ID3D12GraphicsCommandList へのポインタ。nullptr の場合、
/// このメソッドは即座にリターンして何も行いません。
/// </param>
void Material::Bind(ID3D12GraphicsCommandList* commandList) const
{

    if (commandList == nullptr || pipeline_ == nullptr)
    {
        return;
    }

    auto textureHeap = DescriptorHeapManager::Get().GetGlobalTextureHeapAddress();
	auto handle = DescriptorHeapManager::Get().GetGPUHandle(static_cast<DX12Texture*>(m_pTexture)->GetDescriptorIndex());
    commandList->SetPipelineState(pipeline_->pipelineState.Get());
    commandList->SetGraphicsRootSignature(pipeline_->rootSignature.Get());
    commandList->SetDescriptorHeaps(1, textureHeap);
    commandList->SetGraphicsRootDescriptorTable(0, handle);
}
