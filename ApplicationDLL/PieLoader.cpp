#include "pch.h"
#include "PieLoader.h"

#include "AppRuntime.h"
#include "FrameLoop.h"
#include "WinHandleRAII.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace
{
    bool TryLoadPieGameModuleAtPath(const std::filesystem::path& modulePath)
    {
        std::error_code ec;
        if (!std::filesystem::exists(modulePath, ec))
        {
            RuntimeStateRef().g_pieGameLastLoadError = "Module does not exist: " + modulePath.u8string();
            return false;
        }

        const std::filesystem::path hotReloadDir = std::filesystem::temp_directory_path(ec) / L"DirectX12Samples" / L"PieHotReload";
        if (ec)
        {
            RuntimeStateRef().g_pieGameLastLoadError = "temp_directory_path failed";
            return false;
        }

        std::filesystem::create_directories(hotReloadDir, ec);
        if (ec)
        {
            RuntimeStateRef().g_pieGameLastLoadError = "create_directories failed: " + hotReloadDir.u8string();
            return false;
        }

        ++RuntimeStateRef().g_pieHotReloadGeneration;
        const std::filesystem::path hotReloadDllPath = hotReloadDir / (L"PieGameManaged_" + std::to_wstring(RuntimeStateRef().g_pieHotReloadGeneration) + L".dll");
        std::filesystem::copy_file(modulePath, hotReloadDllPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            RuntimeStateRef().g_pieGameLastLoadError = "copy_file failed: " + modulePath.u8string() + " -> " + hotReloadDllPath.u8string();
            return false;
        }

        const std::filesystem::path sourcePdbPath = modulePath.parent_path() / L"PieGameManaged.pdb";
        const std::filesystem::path hotReloadPdbPath = hotReloadDir / (L"PieGameManaged_" + std::to_wstring(RuntimeStateRef().g_pieHotReloadGeneration) + L".pdb");
        std::filesystem::copy_file(sourcePdbPath, hotReloadPdbPath, std::filesystem::copy_options::overwrite_existing, ec);
        ec.clear();

        ScopedModule module(LoadLibraryW(hotReloadDllPath.c_str()));
        if (module.Get() == nullptr)
        {
            RuntimeStateRef().g_pieGameLastLoadError = "LoadLibrary failed. path=" + hotReloadDllPath.u8string() + " gle=" + std::to_string(GetLastError());
            return false;
        }

        PieGameStartFn startFn = reinterpret_cast<PieGameStartFn>(GetProcAddress(module.Get(), "GameStart"));
        PieGameTickFn tickFn = reinterpret_cast<PieGameTickFn>(GetProcAddress(module.Get(), "GameTick"));
        PieGameStopFn stopFn = reinterpret_cast<PieGameStopFn>(GetProcAddress(module.Get(), "GameStop"));
        if (startFn == nullptr || tickFn == nullptr || stopFn == nullptr)
        {
            std::string missing = "Missing export:";
            if (startFn == nullptr) missing += " GameStart";
            if (tickFn == nullptr) missing += " GameTick";
            if (stopFn == nullptr) missing += " GameStop";
            RuntimeStateRef().g_pieGameLastLoadError = missing + " from " + hotReloadDllPath.u8string();
            return false;
        }

        RuntimeStateRef().g_pieGameModule = module.Release();
        RuntimeStateRef().g_pieGameStart = startFn;
        RuntimeStateRef().g_pieGameTick = tickFn;
        RuntimeStateRef().g_pieGameStop = stopFn;
        RuntimeStateRef().g_pieGameModulePath = hotReloadDllPath.u8string();
        RuntimeStateRef().g_pieGameSourceModulePath = modulePath.u8string();
        RuntimeStateRef().g_pieGameSourceWriteTime = std::filesystem::last_write_time(modulePath, ec);
        RuntimeStateRef().g_pieGameSourceWriteTimeValid = !ec;
        RuntimeStateRef().g_pieGameLastLoadError.clear();
        RuntimeStateRef().g_pieGameStatus = "C# game module loaded: " + modulePath.u8string();
        return true;
    }

    void AddPieGameCandidatesFromRoot(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& root)
    {
        const std::filesystem::path pieBinDir = root / L"PieGameManaged" / L"bin";
        std::error_code ec;
        if (!std::filesystem::exists(pieBinDir, ec))
        {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(pieBinDir, ec))
        {
            if (ec)
            {
                break;
            }
            if (!entry.is_regular_file())
            {
                continue;
            }
            if (entry.path().filename() == L"PieGameManaged.dll")
            {
                const auto parentName = entry.path().parent_path().filename().wstring();
                if (parentName == L"native" || parentName == L"publish")
                {
                    candidates.push_back(entry.path());
                }
            }
        }
    }
}

std::filesystem::path GetCurrentModuleDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"ApplicationDLL.dll");
    if (self != nullptr && GetModuleFileNameW(self, modulePath, MAX_PATH) > 0)
    {
        return std::filesystem::path(modulePath).parent_path();
    }

    wchar_t currentDir[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    return std::filesystem::path(currentDir);
}

bool EnsurePieGameModuleLoaded()
{
    if (RuntimeStateRef().g_pieGameModule != nullptr)
    {
        return true;
    }

    const std::filesystem::path moduleDir = GetCurrentModuleDirectory();
    const std::filesystem::path currentDir = std::filesystem::current_path();

    std::vector<std::filesystem::path> candidates;
    candidates.reserve(64);

    for (std::filesystem::path p = currentDir; !p.empty(); p = p.parent_path())
    {
        AddPieGameCandidatesFromRoot(candidates, p);
        if (p == p.parent_path())
        {
            break;
        }
    }
    for (std::filesystem::path p = moduleDir; !p.empty(); p = p.parent_path())
    {
        AddPieGameCandidatesFromRoot(candidates, p);
        if (p == p.parent_path())
        {
            break;
        }
    }

    candidates.push_back(moduleDir / L"native" / L"PieGameManaged.dll");
    candidates.push_back(moduleDir / L"publish" / L"PieGameManaged.dll");
    candidates.push_back(moduleDir / L".." / L".." / L".." / L".." / L"PieGameManaged" / L"bin" / L"Debug" / L"net8.0" / L"win-x64" / L"native" / L"PieGameManaged.dll");
    candidates.push_back(moduleDir / L".." / L".." / L".." / L".." / L"PieGameManaged" / L"bin" / L"Debug" / L"net8.0" / L"win-x64" / L"publish" / L"PieGameManaged.dll");
    candidates.push_back(moduleDir / L".." / L".." / L".." / L".." / L"PieGameManaged" / L"bin" / L"Release" / L"net8.0" / L"win-x64" / L"native" / L"PieGameManaged.dll");
    candidates.push_back(moduleDir / L".." / L".." / L".." / L".." / L"PieGameManaged" / L"bin" / L"Release" / L"net8.0" / L"win-x64" / L"publish" / L"PieGameManaged.dll");

    std::set<std::wstring> seen;
    std::vector<std::filesystem::path> uniqueCandidates;
    uniqueCandidates.reserve(candidates.size());
    for (const auto& candidate : candidates)
    {
        const auto key = candidate.wstring();
        if (seen.insert(key).second)
        {
            uniqueCandidates.push_back(candidate);
        }
    }

    std::sort(uniqueCandidates.begin(), uniqueCandidates.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b)
        {
            std::error_code ecA;
            std::error_code ecB;
            const auto timeA = std::filesystem::last_write_time(a, ecA);
            const auto timeB = std::filesystem::last_write_time(b, ecB);
            if (ecA && ecB)
            {
                return a.wstring() < b.wstring();
            }
            if (ecA)
            {
                return false;
            }
            if (ecB)
            {
                return true;
            }
            return timeA > timeB;
        });

    for (const auto& candidate : uniqueCandidates)
    {
        if (TryLoadPieGameModuleAtPath(candidate))
        {
            return true;
        }
    }

    RuntimeStateRef().g_pieGameStatus = "Invalid or missing C# game module. Need NativeAOT DLL exports: GameStart/GameTick/GameStop.";
    return false;
}

void UnloadPieGameModule()
{
    ScopedModule loadedModule(RuntimeStateRef().g_pieGameModule);
    RuntimeStateRef().g_pieGameModule = nullptr;

    RuntimeStateRef().g_pieGameStart = nullptr;
    RuntimeStateRef().g_pieGameTick = nullptr;
    RuntimeStateRef().g_pieGameStop = nullptr;
    RuntimeStateRef().g_pieGameModulePath.clear();
    RuntimeStateRef().g_pieGameSourceModulePath.clear();
    RuntimeStateRef().g_pieGameSourceWriteTimeValid = false;
    RuntimeStateRef().g_pieGameLastLoadError.clear();
}

bool TryHotReloadPieGameModule()
{
    if (RuntimeStateRef().g_pieGameModule == nullptr || RuntimeStateRef().g_pieGameSourceModulePath.empty() || !RuntimeStateRef().g_pieGameSourceWriteTimeValid)
    {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path sourcePath = std::filesystem::path(RuntimeStateRef().g_pieGameSourceModulePath);
    const auto currentWriteTime = std::filesystem::last_write_time(sourcePath, ec);
    if (ec)
    {
        return false;
    }
    if (currentWriteTime == RuntimeStateRef().g_pieGameSourceWriteTime)
    {
        return false;
    }

    return ReloadPieGameModuleNow("PIE hot reloaded (C#)", "PIE hot reload failed: unable to load updated C# module");
}

bool ReloadPieGameModuleNow(const char* successStatus, const char* failureStatus)
{
    const bool wasRunning = RuntimeStateRef().g_isPieRunning;
    if (wasRunning && RuntimeStateRef().g_pieGameStop != nullptr)
    {
        RuntimeStateRef().g_pieGameStop();
    }
    DestroyAllGameQuads();
    RuntimeStateRef().g_isPieRunning = false;

    UnloadPieGameModule();
    if (!EnsurePieGameModuleLoaded())
    {
        RuntimeStateRef().g_pieGameStatus = failureStatus;
        return false;
    }

    if (wasRunning && RuntimeStateRef().g_pieGameStart != nullptr)
    {
        RuntimeStateRef().g_pieGameStart();
        RuntimeStateRef().g_isPieRunning = true;
    }

    RuntimeStateRef().g_pieGameStatus = successStatus;
    return true;
}

bool ReloadPieGameModuleFromPath(const std::filesystem::path& modulePath, const char* successStatus, const char* failureStatus)
{
    const bool wasRunning = RuntimeStateRef().g_isPieRunning;
    HMODULE oldModule = RuntimeStateRef().g_pieGameModule;
    PieGameStopFn oldStop = RuntimeStateRef().g_pieGameStop;

    if (!TryLoadPieGameModuleAtPath(modulePath))
    {
        RuntimeStateRef().g_pieGameStatus = std::string(failureStatus) + " path=" + modulePath.u8string();
        if (!RuntimeStateRef().g_pieGameLastLoadError.empty())
        {
            RuntimeStateRef().g_pieGameStatus += " detail=" + RuntimeStateRef().g_pieGameLastLoadError;
        }
        return false;
    }

    if (wasRunning && oldStop != nullptr)
    {
        oldStop();
    }
    DestroyAllGameQuads();

    ScopedModule oldModuleHolder(oldModule);

    if (wasRunning && RuntimeStateRef().g_pieGameStart != nullptr)
    {
        RuntimeStateRef().g_pieGameStart();
        RuntimeStateRef().g_isPieRunning = true;
    }
    else
    {
        RuntimeStateRef().g_isPieRunning = wasRunning;
    }

    RuntimeStateRef().g_pieGameStatus = successStatus;
    return true;
}
