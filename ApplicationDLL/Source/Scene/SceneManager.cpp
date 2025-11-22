#include "pch.h"
#include "SceneManager.h"
#include "SceneGame.h"

void SceneManager::ChangeScene(int sceneID)
{
	m_NextSceneID = sceneID;
	// シーン切り替え処理をここに記述
	m_pCurrentScene.reset();
}

void SceneManager::Update(float deltaTime)
{
	// シーンの更新処理をここに記述
	if(m_NextSceneID != m_CurrentSceneID)
	{
		m_CurrentSceneID = m_NextSceneID;
		// ここで新しいシーンを作成して m_pCurrentScene にセットする処理を追加
		switch (m_CurrentSceneID) {
		case 0:
			m_pCurrentScene = std::make_unique<SceneGame>();
			break;
		default:
			break;
		}
	}

	// 現在のシーンが存在する場合は更新を呼び出す
	if (m_pCurrentScene)
	{
		m_pCurrentScene->Update(deltaTime);
	}

}

void SceneManager::Render()
{
	// シーンのレンダリング処理をここに記述
	if (m_pCurrentScene == nullptr)
	{
		return;
	}
	m_pCurrentScene->Render();
}