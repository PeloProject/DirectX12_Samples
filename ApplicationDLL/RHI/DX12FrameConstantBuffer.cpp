
#include "pch.h"
#include "DX12FrameConstantBuffer.h"
#include "Dx12RenderDevice.h"
#include "../Math/MathUtil.h"
#include "DescriptorHeapManager.h"

using namespace WL;

DX12FrameConstantBuffer::DX12FrameConstantBuffer()
{
}

///==================================================================
/// <summary>
/// 初期化処理
/// </summary>
/// <returns></returns>
///==================================================================
bool DX12FrameConstantBuffer::Initialize()
{
	if( IsValid() )
	{
		LOG_DEBUG("Already Initialized");
		return true;
	}
	// 定数バッファリソースの作成
	ID3D12Device* device = Dx12RenderDevice::GetDevice();
	if (device == nullptr)
	{
		LOG_DEBUG("Failed to get DirectX12 device");
		return false;
	}

	const size_t bufferSize = Align(static_cast<size_t>(sizeof(Matrix)), static_cast<size_t>(256));
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pConstantBuffer));

	if (FAILED(result))
	{
		// エラー処理
		LOG_DEBUG("Failed Create ConstantBuffer");
		ResetParam();
		return false;
	}

	result = m_pConstantBuffer->Map(0, nullptr, (void**)&m_pMappedMatrix);//マップ
	if (FAILED(result))
	{
		// エラー処理
		LOG_DEBUG("Failed to map constant buffer: 0x%08X", result);
		ResetParam();
		return false;
	}
	Matrix matrix = DirectX::XMMatrixIdentity();
	*m_pMappedMatrix = matrix;

	// ディスクリプタの確保
	m_DescriptorIndex = DescriptorHeapManager::Get().AllocateGlobalTextureDescriptor();
	if (m_DescriptorIndex == UINT_MAX)
	{
		// エラー処理
		LOG_DEBUG("Failed Constant buffer Allocate Descripter");
		ResetParam();
		return false;
	}

	//定数バッファビューの作成
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(m_pConstantBuffer->GetDesc().Width);
	device->CreateConstantBufferView(&cbvDesc, DescriptorHeapManager::Get().GetCPUHandle(m_DescriptorIndex));

	return true;
}

///==================================================================
/// <summary>
/// 有効かどうかを確認する
/// </summary>
/// <returns></returns>
///==================================================================
bool DX12FrameConstantBuffer::IsValid() const
{
	if( m_pMappedMatrix == nullptr)
	{
		return false;
	}
	if( m_pConstantBuffer == nullptr)
	{
		return false;
	}
	if( m_DescriptorIndex == UINT_MAX)
	{
		return false;
	}

	return true;
}

///==================================================================
/// <summary>
/// パラメータをリセットする
/// </summary>
///==================================================================
void DX12FrameConstantBuffer::ResetParam()
{

	if (m_pMappedMatrix != nullptr && m_pConstantBuffer != nullptr)
	{
		m_pConstantBuffer->Unmap(0, nullptr);
		m_pMappedMatrix = nullptr;
	}

	if (m_DescriptorIndex != UINT_MAX)
	{
		DescriptorHeapManager::Get().FreeGlobalTextureDescriptor(m_DescriptorIndex);
		m_DescriptorIndex = UINT_MAX;
	}

	m_pConstantBuffer.Reset();

}

///==================================================================
/// <summary>
/// デストラクタ
/// </summary>
///==================================================================
DX12FrameConstantBuffer::~DX12FrameConstantBuffer()
{
	ResetParam();
}

void DX12FrameConstantBuffer::Update(const Matrix& matrix)
{
	if( !IsValid() )
	{
		return;
	}

	*m_pMappedMatrix = matrix;
}