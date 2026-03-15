#include "pch.h"
#include "DX12Texture.h"
#include "DescriptorHeapManager.h"
#include "Source/Dx12RenderDevice.h"
#include "RHI/TextureManager.h"

DX12Texture::DX12Texture()
{
	descriptorIndex = TextureManager::Get().CreateTextureResource(m_pTextureBuffer, L"C:/Users/shinji/Documents/Projects/DirectX12_Samples/Assets/Texture/textest.png");

}


void DX12Texture::LoadTexture()
{

}