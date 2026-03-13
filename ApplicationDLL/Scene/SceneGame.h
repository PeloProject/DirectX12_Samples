#pragma once
#include "SceneBase.h"

class SceneGame : public SceneBase
{
public:
	SceneGame();
	virtual ~SceneGame() {}
	virtual void Update(float deltaTime) override;
	virtual void Render() override;
};
