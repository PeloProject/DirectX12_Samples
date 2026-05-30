#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "../Math/MathUtil.h"

using namespace std;
using namespace WL;
using Microsoft::WRL::ComPtr;


struct PMDHeader
{
	float version;		// 例えば1.0や2.0などのバージョン番号
	char modelName[20];
	char comment[256];
};

struct PMDVertex
{
	Vector3 pos;
	Vector3 normal;
	Vector2 uv;
	unsigned short boneIndex[2];
	unsigned char boneWeight;
	unsigned char edgeFlag; // エッジフラグ（0: 通常、1: エッジあり）
}; // PMDファイルの頂点データ構造38バイト

class PMDAnalyzer
{

private:
	static constexpr size_t			PMD_HEADER_SIZE = sizeof(PMDHeader);
	static constexpr unsigned int	PMD_VERTEX_SIZE = 38;//頂点1つあたりのサイズ
	std::vector<PMDVertex>			m_Vertices;
public:
	PMDAnalyzer(string fileName);

	const std::vector<PMDVertex>& GetVertices() const { return m_Vertices; }
};
