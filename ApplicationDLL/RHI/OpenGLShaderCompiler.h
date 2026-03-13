#pragma once
#include "ShaderCompilerBase.h"
class OpenGLShaderCompiler : public IShaderCompiler
{
	public:
	OpenGLShaderCompiler() = default;
	~OpenGLShaderCompiler() override = default;
	HRESULT CompileFromFile() override;
};

