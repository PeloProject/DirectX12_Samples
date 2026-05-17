#pragma once

#include <DirectXMath.h>
namespace WL
{ 
	using Vector3 = DirectX::XMFLOAT3;
	using Vector2 = DirectX::XMFLOAT2;
	using Matrix = DirectX::XMMATRIX;


	inline size_t Align(size_t size, size_t align)
	{
		return (size + align - 1) & ~(align - 1);
	}
};