#pragma once

#include "DX12Texture.h"

#include <filesystem>

using Microsoft::WRL::ComPtr;

class TextureManager
{
public:
	static TextureManager& Get();

	TextureManager(const TextureManager&) = delete;

	/// <summary>
	/// テクスチャーリソースを作成して初期
	/// </summary>
	UINT CreateTextureResource(ComPtr<ID3D12Resource>& textureBuffer, const wchar_t* filePath, DirectX::TexMetadata* outMetadata = nullptr);

	std::filesystem::path ResolveTexturePath(const wchar_t* filePath) const;

	UINT CreateShaderResoureView();

private:
	TextureManager() = default;
	
};

