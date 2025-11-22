#pragma once
#include "SceneBase.h"
#include "PolygonTest.h"

class SceneGame : public SceneBase
{
public:
	SceneGame();
	virtual ~SceneGame() {}
	virtual void Update(float deltaTime) override;
	virtual void Render() override;

private:
	PolygonTest polygonTest;                // ポリゴン描画テスト
};