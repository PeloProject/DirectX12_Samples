#pragma once

#include <d3d12.h>
#include <wrl/client.h>

class DescriptorHeapManager
{
public:
	static DescriptorHeapManager& Get();

	DescriptorHeapManager(const DescriptorHeapManager&) = delete;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVDescriptorHandleForGlobalTextureHeap() const;

	void ResetGlobalTextureHeap()
	{
		m_pGlobalTextureHeap.Reset();
	}

	ID3D12DescriptorHeap* GetGlobalTextureHeap() const
	{
		return m_pGlobalTextureHeap.Get();
	}

private:
	DescriptorHeapManager() = default;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pGlobalTextureHeap;
};

