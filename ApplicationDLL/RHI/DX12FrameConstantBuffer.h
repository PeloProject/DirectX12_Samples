#pragma once

#include "..\Math\MathUtil.h"
#include <wrl/client.h>
#include <d3d12.h>

class DX12FrameConstantBuffer
{
private:
	UINT m_DescriptorIndex = UINT_MAX;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pConstantBuffer;
	WL::Matrix* m_pMappedMatrix = nullptr;

	void ResetParam();
	
public:

	UINT GetDescriptorIndex() const { return m_DescriptorIndex; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_pConstantBuffer ? m_pConstantBuffer->GetGPUVirtualAddress() : 0; }

	bool Initialize();
	void Update(const WL::Matrix& matrix);
	bool IsValid() const;
	
	DX12FrameConstantBuffer();
	~DX12FrameConstantBuffer();

	// コピー禁止
	DX12FrameConstantBuffer(const DX12FrameConstantBuffer&) = delete;
	DX12FrameConstantBuffer& operator=(const DX12FrameConstantBuffer&) = delete;

	// Move禁止
	DX12FrameConstantBuffer(DX12FrameConstantBuffer&&) = delete;
	DX12FrameConstantBuffer& operator=(DX12FrameConstantBuffer&&) = delete;

};
