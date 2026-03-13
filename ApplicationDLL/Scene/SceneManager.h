#pragma once
#include "SceneBase.h"
#include <memory>

class SceneManager
{
public:
	static SceneManager& GetInstance()
	{
		static SceneManager instance;
		return instance;
	}

	void ChangeScene(int sceneID);

	void Update(float deltaTime);
	void Render();

private:
	int m_NextSceneID = -1;
	int m_CurrentSceneID = -1;
	std::unique_ptr<SceneBase> m_pCurrentScene = nullptr;
};