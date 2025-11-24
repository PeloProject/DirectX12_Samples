#pragma once
class Application
{
public:
	static int GetWindowWidth() { return m_WindowWidth; }
	static int GetWindowHeight() { return m_WindowHeight; }
private:

	static constexpr int m_WindowWidth = 400;
	static constexpr int m_WindowHeight = 300;
};

