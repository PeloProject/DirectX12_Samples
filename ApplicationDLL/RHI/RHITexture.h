#pragma once
class RHITexture
{
public:
	RHITexture() = default;
	virtual ~RHITexture() = default;

	virtual void* GetTextureBuffer() const = 0;
};

