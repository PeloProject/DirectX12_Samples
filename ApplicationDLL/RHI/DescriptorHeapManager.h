#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

class DescriptorHeapManager
{
public:
	static DescriptorHeapManager& Get();

	DescriptorHeapManager(const DescriptorHeapManager&) = delete;

	void ResetGlobalTextureHeap()
	{
		m_pGlobalTextureHeap.Reset();
		m_TextureDescriptorFreeList.clear();
		m_TextureFreeIndex = 0;
	}

	ID3D12DescriptorHeap* GetGlobalTextureHeap() const
	{
		return m_pGlobalTextureHeap.Get();
	}

	ID3D12DescriptorHeap** GetGlobalTextureHeapAddress()
	{
		return m_pGlobalTextureHeap.GetAddressOf();
	}

	bool InitializeGlobalTextureHeap(ID3D12Device* device);

	// TODO: Allcate
	UINT AllocateGlobalTextureDescriptor();
	void FreeGlobalTextureDescriptor(UINT descriptorIndex);
	// GetCPUHandle
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;

	// GetGPUHandle
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

private:

	constexpr static UINT MaxGlobalTextureDescriptors = 1000;


	DescriptorHeapManager() = default;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pGlobalTextureHeap;

	UINT m_TextureFreeIndex = 0;
	UINT m_DescriptorSize = 0;

	std::vector<UINT> m_TextureDescriptorFreeList;

	D3D12_CPU_DESCRIPTOR_HANDLE m_CpuStart = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_GpuStart = {};
};

