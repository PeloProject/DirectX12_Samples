#pragma once

class SceneBase
{
public:
	virtual ~SceneBase() {}
	virtual void Update(float deltaTime) = 0;
	virtual void Render() = 0;
};