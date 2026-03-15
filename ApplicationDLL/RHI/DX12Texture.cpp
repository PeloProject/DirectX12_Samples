#include "pch.h"
#include "DX12Texture.h"
#include "RHI/TextureManager.h"

DX12Texture::DX12Texture()
{
}

bool DX12Texture::LoadFromFile(const wchar_t* filePath)
{
	if (filePath == nullptr)
	{
		return false;
	}

	const UINT newDescriptorIndex = TextureManager::Get().CreateTextureResource(m_pTextureBuffer, filePath, &m_Metadata);
	if (newDescriptorIndex == static_cast<UINT>(-1))
	{
		return false;
	}

	descriptorIndex = newDescriptorIndex;
	return true;
}

bool DX12Texture::LoadFromFile(const std::wstring& filePath)
{
	return LoadFromFile(filePath.c_str());
}
