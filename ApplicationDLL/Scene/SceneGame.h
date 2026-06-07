#pragma once
#include "SceneBase.h"

class SceneGame : public SceneBase
{
public:
	SceneGame();
	virtual ~SceneGame() {}
	virtual void Update(float deltaTime) override;
	virtual void Render(ViewportRenderMode viewportMode) override;

private:
	//std::unique_ptr<PMDRenderObject> m_ppmdModel_;
};
