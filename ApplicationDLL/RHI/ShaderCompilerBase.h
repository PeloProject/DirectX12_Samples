#pragma once
#include <d3dcompiler.h>
#include <wrl/client.h>
class IShaderCompiler
{
public:
	IShaderCompiler() = default;
	virtual ~IShaderCompiler(){}


    virtual HRESULT CompileFromFile() = 0;
};

