
#include "pch.h"
#include "PMDAnalyzer.h"
#include "Source/Dx12RenderDevice.h"
#include <d3d12.h>
#include "d3dx12.h" // ヘッダーをインクルード

PMDAnalyzer::PMDAnalyzer(string fileName)
{
	PMDHeader header;
	char signature[3];
	FILE* fp;
	auto err = fopen_s(&fp, fileName.c_str(), "rb");
	if (err != 0) {
		// エラー処理
		return;
	}

	// headerの読み込み
	fread(signature, sizeof(signature), 1, fp);
	fread(&header, sizeof(header), 1, fp);

	// データの読み込みや解析をここで行う
	unsigned int vertexCount;
	fread(&vertexCount, sizeof(vertexCount), 1, fp);

	std::vector<unsigned char> vertices(vertexCount * sizeof(PMDVertex));
	fread(vertices.data(), vertices.size(), 1, fp);

	// 頂点バッファの作成
	//HRESULT hr =Dx12RenderDevice::GetDevice()->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//	D3D12_HEAP_FLAG_NONE,
	//	&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&m_pVertexBuffer)
	//);

	fclose(fp);
}