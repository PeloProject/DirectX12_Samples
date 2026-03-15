#pragma once

#include "DX12Texture.h"



using Microsoft::WRL::ComPtr;

class TextureManager
{
public:
	static TextureManager& Get();

	TextureManager(const TextureManager&) = delete;

	/// <summary>
	/// テクスチャーリソースを作成して初期
	/// </summary>
	UINT CreateTextureResource(ComPtr<ID3D12Resource>& textureBuffer, const wchar_t* filePath);

	UINT CreateShaderResoureView();

	// CreateSRV()
private:
	TextureManager() = default;
	
};

