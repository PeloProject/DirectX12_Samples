
#include "pch.h"
#include "PMDAnalyzer.h"
#include "Source/Dx12RenderDevice.h"
#include <d3d12.h>
#include <utility>
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

	std::vector<PMDVertex> vertices(vertexCount);

	for (unsigned int i = 0; i < vertexCount; i++)
	{
		fread(&vertices[i], PMD_VERTEX_SIZE, 1, fp);
	}

	m_Vertices = std::move(vertices);

	fclose(fp);
}
