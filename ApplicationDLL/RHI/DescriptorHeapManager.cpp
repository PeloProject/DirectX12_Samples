#include "DescriptorHeapManager.h"

DescriptorHeapManager& DescriptorHeapManager::Get()
{
    static DescriptorHeapManager instance;
    return instance;
}

/// <summary>
/// CBV/SRV/UAV タイプのシェーダー可視なグローバルテクスチャ用記述子ヒープを作成して初期化します。
/// 作成されたヒープは m_pGlobalTextureHeap メンバに格納されます。
/// </summary>
/// <param name="device">ID3D12Device へのポインタ。記述子ヒープの作成に使用されます。</param>
/// <param name="numDescriptors">作成する記述子の総数。ヒープに含めるデスクリプタ数を指定します。</param>
/// <returns>初期化に成功した場合は true、CreateDescriptorHeap の呼び出しに失敗した場合は false を返します。</returns>
bool DescriptorHeapManager::InitializeGlobalTextureHeap(ID3D12Device* device)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = MaxGlobalTextureDescriptors;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pGlobalTextureHeap.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		//("Failed to create global texture descriptor heap.");
		return false;
	}

	m_DescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_CpuStart = m_pGlobalTextureHeap->GetCPUDescriptorHandleForHeapStart();
	m_GpuStart = m_pGlobalTextureHeap->GetGPUDescriptorHandleForHeapStart();

	return true;
}

UINT DescriptorHeapManager::AllocateGlobalTextureDescriptor()
{
	if (!m_TextureDescriptorFreeList.empty())
	{
		UINT freeIndex = m_TextureDescriptorFreeList.back();
		m_TextureDescriptorFreeList.pop_back();
		return freeIndex;
	}

	if (m_TextureFreeIndex < MaxGlobalTextureDescriptors)
	{
		return m_TextureFreeIndex++;
	}
}

void DescriptorHeapManager::FreeGlobalTextureDescriptor(UINT descriptorIndex)
{
	if (descriptorIndex < MaxGlobalTextureDescriptors)
	{
		m_TextureDescriptorFreeList.push_back(descriptorIndex);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetCPUHandle(uint32_t index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_CpuStart;
	handle.ptr += static_cast<SIZE_T>(index) * m_DescriptorSize;
	return handle;
}

///====================================================================
/// <summary>
/// 指定されたインデックスに対応する GPU デスクリプタハンドルを取得します。	
/// </summary>
/// <param name="index">取得するデスクリプタのインデックス。</param>
/// <returns>指定したインデックスに対応する D3D12_GPU_DESCRIPTOR_HANDLE を返します。m_GpuStart から index × m_DescriptorSize バイト分オフセットしたハンドルです。</returns>
///====================================================================
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetGPUHandle(UINT index) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = m_GpuStart;
	handle.ptr += static_cast<UINT64>(index) * m_DescriptorSize;
	return handle;
}