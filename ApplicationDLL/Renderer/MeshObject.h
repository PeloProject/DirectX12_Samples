#pragma once
#include "pch.h"
#include <string>
#include <cstdint>
#include "../Math/MathUtil.h"

using namespace std;
using namespace WL;
using Microsoft::WRL::ComPtr;

class MeshObject
{
public:
	MeshObject(string fileName);

private:
	ComPtr<ID3D12Resource>	m_pVertexBuffer;

	D3D12_VERTEX_BUFFER_VIEW	m_VertexBufferView = {};
};

