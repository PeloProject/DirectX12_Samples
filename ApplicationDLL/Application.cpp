#include "pch.h"
#include "Application.h"

int Application::m_WindowWidth = 400;
int Application::m_WindowHeight = 300;

void Application::SetWindowSize(int width, int height)
{
	if (width > 0)
	{
		m_WindowWidth = width;
	}

	if (height > 0)
	{
		m_WindowHeight = height;
	}
}
