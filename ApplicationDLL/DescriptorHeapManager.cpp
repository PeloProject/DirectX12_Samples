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