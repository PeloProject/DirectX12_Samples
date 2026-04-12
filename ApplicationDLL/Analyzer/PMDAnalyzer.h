#pragma once

#include <string>
#include "../Math/MathUtil.h"

using namespace std;
using namespace WL;

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
	static constexpr size_t PMD_HEADER_SIZE = sizeof(PMDHeader);
public:
	PMDAnalyzer(string fileName);
};