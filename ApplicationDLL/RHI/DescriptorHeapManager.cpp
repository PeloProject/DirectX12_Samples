#include "DescriptorHeapManager.h"

DescriptorHeapManager& DescriptorHeapManager::Get()
{
    static DescriptorHeapManager instance;
    return instance;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetSRVDescriptorHandleForGlobalTextureHeap() const
{
	if (m_pGlobalTextureHeap == nullptr)
	{
		return { 0 };
	}
	return m_pGlobalTextureHeap->GetGPUDescriptorHandleForHeapStart();
}

bool DescriptorHeapManager::InitializeGlobalTextureHeap(ID3D12Device* device, UINT numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pGlobalTextureHeap.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		//("Failed to create global texture descriptor heap.");
		return false;
	}
	return true;
}