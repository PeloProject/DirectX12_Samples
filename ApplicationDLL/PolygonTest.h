#pragma once
#include "MathUtil.h"

using namespace WL;

class PolygonTest
{

public:
	PolygonTest();
	~PolygonTest() {}


private:

	void CreateGpuResources();

	void ApplyTransformations();

	Vector3 m_Vertices[3];
};

