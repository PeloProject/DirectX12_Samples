#pragma once
class Application
{
public:
	static int GetWindowWidth() { return m_WindowWidth; }
	static int GetWindowHeight() { return m_WindowHeight; }
	static void SetWindowSize(int width, int height);
private:

	static int m_WindowWidth;
	static int m_WindowHeight;
};

