#include "pch.h"
#include "OpenGLShaderCompiler.h"

#include "Source/OpenGLLoader.h"
#include <glad/glad.h>

#include <array>
#include <filesystem>

namespace
{
    std::filesystem::path GetModuleDirectory()
    {
        wchar_t modulePath[MAX_PATH] = {};
        HMODULE hModule = GetModuleHandleW(L"ApplicationDLL.dll");
        if (hModule != nullptr && GetModuleFileNameW(hModule, modulePath, MAX_PATH) > 0)
        {
            return std::filesystem::path(modulePath).parent_path();
        }

        wchar_t currentDir[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, currentDir);
        return std::filesystem::path(currentDir);
    }

    std::filesystem::path ResolveShaderPath(const wchar_t* shaderFileName)
    {
        const std::filesystem::path moduleDir = GetModuleDirectory();
        const std::filesystem::path currentDir = std::filesystem::current_path();
        const std::array<std::filesystem::path, 4> candidates = {
            moduleDir / L".." / L".." / L".." / L".." / L"ApplicationDLL" / L"Shader" / shaderFileName,
            moduleDir / L"ApplicationDLL" / L"Shader" / shaderFileName,
            moduleDir / L"Shader" / shaderFileName,
            std::filesystem::path(L"Shader") / shaderFileName
        };
        const std::array<std::filesystem::path, 3> extraCandidates = {
            moduleDir / L".." / L"ApplicationDLL" / L"Shader" / shaderFileName,
            currentDir / L"ApplicationDLL" / L"Shader" / shaderFileName,
            currentDir / L"Shader" / shaderFileName
        };

        for (const auto& path : candidates)
        {
            std::error_code ec;
            if (std::filesystem::exists(path, ec))
            {
                return std::filesystem::weakly_canonical(path, ec);
            }
        }

        for (const auto& path : extraCandidates)
        {
            std::error_code ec;
            if (std::filesystem::exists(path, ec))
            {
                return std::filesystem::weakly_canonical(path, ec);
            }
        }

        return extraCandidates.back();
    }
}

HRESULT OpenGLShaderCompiler::CompileFromFile()
{
	// OpenGLのシェーダーコンパイル処理をここに実装する
	// 例えば、glCompileShaderやglLinkProgramなどのOpenGL関数を使用してシェーダーをコンパイルするコードを記述することができます。
	const std::filesystem::path shaderPath = ResolveShaderPath(L"shaderFileName");
	

    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    //glShaderSource(shader, sizeof source / sizeof source[0], source, 0);
    //glCompileShader(shader);

	return S_OK; // 成功した場合はS_OKを返す
}
