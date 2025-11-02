#pragma once

#include <Windows.h>
class GraphicsDevice
{
public:
	virtual ~GraphicsDevice() {}

	virtual bool Initialize(HWND hwnd) = 0;


};

