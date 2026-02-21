#include "pch.h"
#include "PieAutoPublish.h"

#include "AppRuntime.h"
#include "PieLoader.h"
#include "WinHandleRAII.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace
{
    bool PreparePieManagedAutoPublishWorkspace(std::filesystem::path& outWorkspaceDir, std::string& outErrorMessage)
    {
        if (!ResolvePieManagedCsprojPath())
        {
            outErrorMessage = "csproj path not found";
            return false;
        }

        std::error_code ec;
        const std::filesystem::path tempRoot = std::filesystem::temp_directory_path(ec);
        if (ec)
        {
            outErrorMessage = "temp_directory_path failed";
            return false;
        }

        const std::filesystem::path workspaceRoot = tempRoot / L"DirectX12Samples" / L"PieAutoPublishWorkspace";
        std::filesystem::create_directories(workspaceRoot, ec);
        if (ec)
        {
            outErrorMessage = "create workspace root failed";
            return false;
        }

        const std::filesystem::path sourceDir = RuntimeStateRef().g_pieManagedCsprojPath.parent_path();
        outWorkspaceDir = workspaceRoot / (L"ws_" + std::to_wstring(++RuntimeStateRef().g_pieManagedAutoPublishGeneration));
        std::filesystem::create_directories(outWorkspaceDir, ec);
        if (ec)
        {
            outErrorMessage = "create workspace dir failed";
            return false;
        }

        std::filesystem::recursive_directory_iterator it(sourceDir, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                outErrorMessage = "workspace copy iteration failed";
                return false;
            }

            const auto& entry = *it;
            const auto filename = entry.path().filename().wstring();
            if (entry.is_directory())
            {
                if (filename == L"bin" || filename == L"obj" || filename == L".vs")
                {
                    it.disable_recursion_pending();
                }
                continue;
            }

            if (!entry.is_regular_file())
            {
                continue;
            }

            std::filesystem::path relativePath = std::filesystem::relative(entry.path(), sourceDir, ec);
            if (ec)
            {
                outErrorMessage = "relative path resolve failed";
                return false;
            }

            const std::filesystem::path destination = outWorkspaceDir / relativePath;
            std::filesystem::create_directories(destination.parent_path(), ec);
            if (ec)
            {
                outErrorMessage = "create destination dir failed";
                return false;
            }

            std::filesystem::copy_file(entry.path(), destination, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                outErrorMessage = "copy file failed: " + entry.path().u8string();
                return false;
            }
        }

        return true;
    }
}

bool ResolvePieManagedCsprojPath()
{
    if (!RuntimeStateRef().g_pieManagedCsprojPath.empty())
    {
        return true;
    }

    const std::filesystem::path moduleDir = GetCurrentModuleDirectory();
    const std::filesystem::path currentDir = std::filesystem::current_path();
    std::vector<std::filesystem::path> searchRoots;
    searchRoots.reserve(16);

    for (std::filesystem::path p = currentDir; !p.empty(); p = p.parent_path())
    {
        searchRoots.push_back(p);
        if (p == p.parent_path())
        {
            break;
        }
    }
    for (std::filesystem::path p = moduleDir; !p.empty(); p = p.parent_path())
    {
        searchRoots.push_back(p);
        if (p == p.parent_path())
        {
            break;
        }
    }

    std::set<std::wstring> seen;
    for (const auto& root : searchRoots)
    {
        const std::wstring key = root.wstring();
        if (!seen.insert(key).second)
        {
            continue;
        }

        const std::filesystem::path csprojPath = root / L"PieGameManaged" / L"PieGameManaged.csproj";
        std::error_code ec;
        if (std::filesystem::exists(csprojPath, ec))
        {
            RuntimeStateRef().g_pieManagedCsprojPath = csprojPath;
            return true;
        }
    }

    return false;
}

bool TryGetPieManagedSourceWriteTime(std::filesystem::file_time_type& outWriteTime)
{
    if (!ResolvePieManagedCsprojPath())
    {
        return false;
    }

    const std::filesystem::path projectDir = RuntimeStateRef().g_pieManagedCsprojPath.parent_path();
    std::error_code ec;
    if (!std::filesystem::exists(projectDir, ec))
    {
        return false;
    }

    bool hasAny = false;
    std::filesystem::file_time_type latestWriteTime = {};
    std::filesystem::recursive_directory_iterator it(projectDir, ec);
    std::filesystem::recursive_directory_iterator end;
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        const auto& entry = *it;
        if (entry.is_directory())
        {
            const auto dirName = entry.path().filename().wstring();
            if (dirName == L"bin" || dirName == L"obj")
            {
                it.disable_recursion_pending();
                continue;
            }
            continue;
        }
        if (!entry.is_regular_file())
        {
            continue;
        }

        const auto ext = entry.path().extension().wstring();
        const bool isSourceFile =
            ext == L".cs" || ext == L".csproj" || ext == L".props" || ext == L".targets" || ext == L".json";
        if (!isSourceFile)
        {
            continue;
        }

        const auto writeTime = entry.last_write_time(ec);
        if (ec)
        {
            continue;
        }

        if (!hasAny || writeTime > latestWriteTime)
        {
            latestWriteTime = writeTime;
            hasAny = true;
        }
    }

    if (!hasAny)
    {
        return false;
    }

    outWriteTime = latestWriteTime;
    return true;
}

bool TryStartPieManagedAutoPublish()
{
    if (RuntimeStateRef().g_pieManagedPublishProcess != nullptr)
    {
        return false;
    }
    if (!ResolvePieManagedCsprojPath())
    {
        return false;
    }

    std::filesystem::path workspaceDir = {};
    std::string workspaceError = {};
    if (!PreparePieManagedAutoPublishWorkspace(workspaceDir, workspaceError))
    {
        RuntimeStateRef().g_pieGameStatus = "PIE auto-publish workspace failed: " + workspaceError;
        return false;
    }

    const std::filesystem::path projectDir = workspaceDir;
    RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath = projectDir / L"out" / L"native" / L"PieGameManaged.dll";

    std::error_code ec;
    const std::filesystem::path logDir = std::filesystem::temp_directory_path(ec) / L"DirectX12Samples" / L"PieAutoPublishLogs";
    if (ec)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE auto-publish failed to resolve log directory";
        return false;
    }
    std::filesystem::create_directories(logDir, ec);
    if (ec)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE auto-publish failed to create log directory";
        return false;
    }

    const std::filesystem::path logPath = logDir / (L"publish_" + std::to_wstring(GetTickCount64()) + L".log");
    RuntimeStateRef().g_pieManagedLastPublishLogPath = logPath.u8string();

    const std::wstring commandLine =
        L"cmd /c dotnet publish PieGameManaged.csproj -c Debug -r win-x64 /p:NativeLib=Shared /p:SelfContained=true /p:PublishAot=true /p:UseCurrentRuntimeIdentifier=true /p:OutputPath=out > \""
        + logPath.wstring() + L"\" 2>&1";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::wstring mutableCommand = commandLine;
    const BOOL created = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        projectDir.c_str(),
        &si,
        &pi);

    if (!created)
    {
        RuntimeStateRef().g_pieGameStatus = "PIE auto-publish failed to start";
        return false;
    }

    ScopedHandle threadHandle(pi.hThread);
    RuntimeStateRef().g_pieManagedPublishProcess = pi.hProcess;
    RuntimeStateRef().g_pieGameStatus = "PIE auto-publish running...";
    return true;
}

void TickPieManagedAutoPublish(float deltaTime)
{
    if (!RuntimeStateRef().g_isPieRunning)
    {
        return;
    }

    if (RuntimeStateRef().g_pieManagedPublishProcess != nullptr)
    {
        const DWORD waitResult = WaitForSingleObject(RuntimeStateRef().g_pieManagedPublishProcess, 0);
        if (waitResult == WAIT_OBJECT_0)
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(RuntimeStateRef().g_pieManagedPublishProcess, &exitCode);
            ScopedHandle finished(RuntimeStateRef().g_pieManagedPublishProcess);
            RuntimeStateRef().g_pieManagedPublishProcess = nullptr;

            if (exitCode == 0)
            {
                if (RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid)
                {
                    RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTime = RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTime;
                    RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTimeValid = true;
                }

                bool reloaded = false;
                const auto reloadCandidates = BuildPieManagedReloadCandidates();
                for (const auto& candidate : reloadCandidates)
                {
                    if (ReloadPieGameModuleFromPath(candidate, "PIE auto-publish completed + reloaded", "PIE auto-publish completed, but reload failed"))
                    {
                        reloaded = true;
                        break;
                    }
                }
                if (!reloaded)
                {
                    RuntimeStateRef().g_pieGameStatus = "PIE auto-publish completed, but no newer reload candidate was found";
                }
            }
            else
            {
                RuntimeStateRef().g_pieGameStatus = "PIE auto-publish failed (exit code=" + std::to_string(exitCode) + ")";
                if (!RuntimeStateRef().g_pieManagedLastPublishLogPath.empty())
                {
                    RuntimeStateRef().g_pieGameStatus += " log=" + RuntimeStateRef().g_pieManagedLastPublishLogPath;
                }
            }
            RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid = false;
        }
    }

    RuntimeStateRef().g_pieManagedPublishCheckTimer += deltaTime;
    if (RuntimeStateRef().g_pieManagedPublishCheckTimer < 1.0f)
    {
        return;
    }
    RuntimeStateRef().g_pieManagedPublishCheckTimer = 0.0f;

    std::filesystem::file_time_type currentSourceWriteTime = {};
    if (!TryGetPieManagedSourceWriteTime(currentSourceWriteTime))
    {
        return;
    }

    if (!RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTimeValid)
    {
        RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTime = currentSourceWriteTime;
        RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTimeValid = true;
        return;
    }

    if (currentSourceWriteTime <= RuntimeStateRef().g_pieManagedLastPublishedSourceWriteTime)
    {
        return;
    }

    if (TryStartPieManagedAutoPublish())
    {
        RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTime = currentSourceWriteTime;
        RuntimeStateRef().g_pieManagedPendingPublishSourceWriteTimeValid = true;
    }
}

std::vector<std::filesystem::path> BuildPieManagedReloadCandidates()
{
    std::vector<std::filesystem::path> candidates;

    if (!RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath.empty())
    {
        candidates.push_back(RuntimeStateRef().g_pieManagedHotReloadPublishedDllPath);
    }

    if (ResolvePieManagedCsprojPath())
    {
        const auto projectDir = RuntimeStateRef().g_pieManagedCsprojPath.parent_path();
        candidates.push_back(projectDir / L"bin" / L"Debug" / L"net8.0" / L"win-x64" / L"native" / L"PieGameManaged.dll");
        candidates.push_back(projectDir / L"bin" / L"Debug" / L"net8.0" / L"win-x64" / L"publish" / L"PieGameManaged.dll");
        candidates.push_back(projectDir / L"bin" / L"Release" / L"net8.0" / L"win-x64" / L"native" / L"PieGameManaged.dll");
        candidates.push_back(projectDir / L"bin" / L"Release" / L"net8.0" / L"win-x64" / L"publish" / L"PieGameManaged.dll");
    }

    std::set<std::wstring> seen;
    std::vector<std::filesystem::path> uniqueCandidates;
    uniqueCandidates.reserve(candidates.size());
    for (const auto& candidate : candidates)
    {
        std::error_code ecExists;
        if (!std::filesystem::exists(candidate, ecExists))
        {
            continue;
        }
        if (ecExists)
        {
            continue;
        }

        if (RuntimeStateRef().g_pieGameSourceWriteTimeValid)
        {
            std::error_code ecTime;
            const auto candidateTime = std::filesystem::last_write_time(candidate, ecTime);
            if (ecTime)
            {
                continue;
            }
            if (candidateTime <= RuntimeStateRef().g_pieGameSourceWriteTime)
            {
                continue;
            }
        }

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

    return uniqueCandidates;
}
