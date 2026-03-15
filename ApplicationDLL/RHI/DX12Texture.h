#pragma once
#include "RHITexture.h"
#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class DX12Texture : public RHITexture
{
public:
	DX12Texture();
	~DX12Texture() override = default;

	bool LoadFromFile(const wchar_t* filePath);
	bool LoadFromFile(const std::wstring& filePath);

	void* GetTextureBuffer() const override { return m_pTextureBuffer.Get(); }
	DirectX::TexMetadata GetMetadata() const { return m_Metadata; }
	UINT GetDescriptorIndex() const { return descriptorIndex; }
private:

	/// <summary>
	/// Direct3D 12 のテクスチャリソースを参照する ComPtr<ID3D12Resource> 型のメンバ変数。
	/// </summary>
	ComPtr<ID3D12Resource>		m_pTextureBuffer;

	UINT descriptorIndex = -1;

	DirectX::TexMetadata m_Metadata = {};
};

