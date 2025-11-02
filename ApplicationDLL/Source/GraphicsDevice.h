#pragma once

#include <Windows.h>
class GraphicsDevice
{
public:
	virtual ~GraphicsDevice() {}

	virtual bool Initialize(HWND hwnd, UINT width, UINT height) = 0;


};

