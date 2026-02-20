// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include "Source/DirectXDevice.h"
#include "Source/EditorUi.h"
#include "SceneManager.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>


//BOOL APIENTRY DllMain( HMODULE hModule,
//                       DWORD  ul_reason_for_call,
//                       LPVOID lpReserved
//                     )
//{
//    switch (ul_reason_for_call)
//    {
//    case DLL_PROCESS_ATTACH:
//    case DLL_THREAD_ATTACH:
//    case DLL_THREAD_DETACH:
//    case DLL_PROCESS_DETACH:
//        break;
//    }
//    return TRUE;
//}

#include <tchar.h>
#include <PolygonTest.h>

static HWND g_hwnd = NULL;
static DirectXDevice g_DxDevice;
static bool g_imguiInitialized = false;
using PieTickCallback = void(__cdecl*)(float);
static PieTickCallback g_pieTickCallback = nullptr;
static bool g_isPieRunning = false;
static bool g_isStandaloneMode = false;
static bool g_pendingStartPie = false;
static bool g_pendingStopPie = false;
static HMODULE g_pieGameModule = nullptr;
using PieGameStartFn = void(__cdecl*)();
using PieGameTickFn = void(__cdecl*)(float);
using PieGameStopFn = void(__cdecl*)();
static PieGameStartFn g_pieGameStart = nullptr;
static PieGameTickFn g_pieGameTick = nullptr;
static PieGameStopFn g_pieGameStop = nullptr;
static std::string g_pieGameStatus = "PIE module not loaded";
static std::string g_pieGameModulePath = "";
static std::string g_pieGameSourceModulePath = "";
static std::string g_pieGameLastLoadError = "";
static std::filesystem::file_time_type g_pieGameSourceWriteTime = {};
static bool g_pieGameSourceWriteTimeValid = false;
static std::filesystem::path g_pieManagedCsprojPath = {};
static std::filesystem::path g_pieManagedHotReloadPublishedDllPath = {};
static std::filesystem::file_time_type g_pieManagedLastPublishedSourceWriteTime = {};
static bool g_pieManagedLastPublishedSourceWriteTimeValid = false;
static std::filesystem::file_time_type g_pieManagedPendingPublishSourceWriteTime = {};
static bool g_pieManagedPendingPublishSourceWriteTimeValid = false;
static HANDLE g_pieManagedPublishProcess = nullptr;
static std::string g_pieManagedLastPublishLogPath = "";
static uint64_t g_pieManagedAutoPublishGeneration = 0;
static uint64_t g_pieHotReloadGeneration = 0;
static float g_pieHotReloadCheckTimer = 0.0f;
static float g_pieManagedPublishCheckTimer = 0.0f;
static float g_gameClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static std::unordered_map<uint32_t, std::unique_ptr<PolygonTest>> g_gameQuads;
static uint32_t g_nextGameQuadHandle = 1;
static constexpr float kDefaultSceneClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static void ConfigureD3D12DebugFilters();
static void RenderGameQuads();
static void DestroyAllGameQuads();
static bool EnsurePieGameModuleLoaded();
static std::filesystem::path GetCurrentModuleDirectory();
static bool TryLoadPieGameModuleAtPath(const std::filesystem::path& modulePath);
static void AddPieGameCandidatesFromRoot(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& root);
static void UnloadPieGameModule();
static bool TryHotReloadPieGameModule();
static bool ReloadPieGameModuleNow(const char* successStatus, const char* failureStatus);
static bool ReloadPieGameModuleFromPath(const std::filesystem::path& modulePath, const char* successStatus, const char* failureStatus);
static bool ResolvePieManagedCsprojPath();
static bool TryGetPieManagedSourceWriteTime(std::filesystem::file_time_type& outWriteTime);
static bool PreparePieManagedAutoPublishWorkspace(std::filesystem::path& outWorkspaceDir, std::string& outErrorMessage);
static bool TryStartPieManagedAutoPublish();
static void TickPieManagedAutoPublish(float deltaTime);
static void StartPieImmediate();
static void StopPieImmediate();
static std::vector<std::filesystem::path> BuildPieManagedReloadCandidates();
extern "C" __declspec(dllexport) void StartPie();
extern "C" __declspec(dllexport) void StopPie();
extern "C" __declspec(dllexport) void SetStandaloneMode(BOOL enabled);
extern "C" __declspec(dllexport) uint32_t CreateGameQuad();
extern "C" __declspec(dllexport) void DestroyGameQuad(uint32_t handle);
extern "C" __declspec(dllexport) void SetGameQuadTransform(uint32_t handle, float centerX, float centerY, float width, float height);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        if (hwnd == g_hwnd)
        {
            const UINT width = LOWORD(lParam);
            const UINT height = HIWORD(lParam);
            if (wParam != SIZE_MINIMIZED && width > 0 && height > 0)
            {
                Application::SetWindowSize(static_cast<int>(width), static_cast<int>(height));
                EditorUi::RequestSceneRenderSize(width, height);
                g_DxDevice.Resize(width, height);
                if (g_imguiInitialized)
                {
                    EditorUi::EnsureSceneRenderSize();
                }
            }
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        EditorUi::Shutdown();
        g_imguiInitialized = false;
        g_DxDevice.Shutdown();
        g_hwnd = NULL;
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // 背景を塗りつぶし
        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        // テキスト描画
        const TCHAR* text = _T("Hello from C++!");
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        if (g_imguiInitialized && EditorUi::HandleWndProc(hwnd, msg, wParam, lParam))
        {
            return 1;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static bool InitializeImGui()
{
    return EditorUi::Initialize(
        g_hwnd,
        g_DxDevice.GetCommandQueue(),
        static_cast<UINT>(Application::GetWindowWidth()),
        static_cast<UINT>(Application::GetWindowHeight()));
}

static void ShutdownImGui()
{
    EditorUi::Shutdown();
    g_imguiInitialized = false;
}

static bool CreateOrResizeSceneRenderTexture(UINT width, UINT height)
{
    EditorUi::RequestSceneRenderSize(width, height);
    return EditorUi::EnsureSceneRenderSize();
}

static void DestroySceneRenderTexture()
{
    EditorUi::RequestSceneRenderSize(1, 1);
}

static void BeginSceneRenderToTexture()
{
    EditorUi::BeginSceneRenderToTexture(g_isPieRunning, g_gameClearColor, kDefaultSceneClearColor);
}

static void EndSceneRenderToTexture()
{
    EditorUi::EndSceneRenderToTexture();
}

static void ConfigureD3D12DebugFilters()
{
    ID3D12Device* device = DirectXDevice::GetDevice();
    if (device == nullptr)
    {
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        return;
    }

    D3D12_MESSAGE_ID denyIds[] = {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
    };

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    infoQueue->AddStorageFilterEntries(&filter);
}

static void RenderGameQuads()
{
    for (auto& quadEntry : g_gameQuads)
    {
        if (quadEntry.second != nullptr)
        {
            quadEntry.second->Render();
        }
    }
}

static void DestroyAllGameQuads()
{
    g_gameQuads.clear();
    g_nextGameQuadHandle = 1;
}

static std::filesystem::path GetCurrentModuleDirectory()
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

static bool TryLoadPieGameModuleAtPath(const std::filesystem::path& modulePath)
{
    std::error_code ec;
    if (!std::filesystem::exists(modulePath, ec))
    {
        g_pieGameLastLoadError = "Module does not exist: " + modulePath.u8string();
        return false;
    }

    const std::filesystem::path hotReloadDir = std::filesystem::temp_directory_path(ec) / L"DirectX12Samples" / L"PieHotReload";
    if (ec)
    {
        g_pieGameLastLoadError = "temp_directory_path failed";
        return false;
    }

    std::filesystem::create_directories(hotReloadDir, ec);
    if (ec)
    {
        g_pieGameLastLoadError = "create_directories failed: " + hotReloadDir.u8string();
        return false;
    }

    ++g_pieHotReloadGeneration;
    const std::filesystem::path hotReloadDllPath = hotReloadDir / (L"PieGameManaged_" + std::to_wstring(g_pieHotReloadGeneration) + L".dll");
    std::filesystem::copy_file(modulePath, hotReloadDllPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        g_pieGameLastLoadError = "copy_file failed: " + modulePath.u8string() + " -> " + hotReloadDllPath.u8string();
        return false;
    }

    const std::filesystem::path sourcePdbPath = modulePath.parent_path() / L"PieGameManaged.pdb";
    const std::filesystem::path hotReloadPdbPath = hotReloadDir / (L"PieGameManaged_" + std::to_wstring(g_pieHotReloadGeneration) + L".pdb");
    std::filesystem::copy_file(sourcePdbPath, hotReloadPdbPath, std::filesystem::copy_options::overwrite_existing, ec);
    ec.clear();

    HMODULE module = LoadLibraryW(hotReloadDllPath.c_str());
    if (module == nullptr)
    {
        g_pieGameLastLoadError = "LoadLibrary failed. path=" + hotReloadDllPath.u8string() + " gle=" + std::to_string(GetLastError());
        return false;
    }

    PieGameStartFn startFn = reinterpret_cast<PieGameStartFn>(GetProcAddress(module, "GameStart"));
    PieGameTickFn tickFn = reinterpret_cast<PieGameTickFn>(GetProcAddress(module, "GameTick"));
    PieGameStopFn stopFn = reinterpret_cast<PieGameStopFn>(GetProcAddress(module, "GameStop"));
    if (startFn == nullptr || tickFn == nullptr || stopFn == nullptr)
    {
        std::string missing = "Missing export:";
        if (startFn == nullptr) missing += " GameStart";
        if (tickFn == nullptr) missing += " GameTick";
        if (stopFn == nullptr) missing += " GameStop";
        g_pieGameLastLoadError = missing + " from " + hotReloadDllPath.u8string();
        FreeLibrary(module);
        return false;
    }

    g_pieGameModule = module;
    g_pieGameStart = startFn;
    g_pieGameTick = tickFn;
    g_pieGameStop = stopFn;
    g_pieGameModulePath = hotReloadDllPath.u8string();
    g_pieGameSourceModulePath = modulePath.u8string();
    g_pieGameSourceWriteTime = std::filesystem::last_write_time(modulePath, ec);
    g_pieGameSourceWriteTimeValid = !ec;
    g_pieGameLastLoadError.clear();
    g_pieGameStatus = "C# game module loaded: " + modulePath.u8string();
    return true;
}

static void AddPieGameCandidatesFromRoot(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& root)
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

static bool ResolvePieManagedCsprojPath()
{
    if (!g_pieManagedCsprojPath.empty())
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
            g_pieManagedCsprojPath = csprojPath;
            return true;
        }
    }

    return false;
}

static bool TryGetPieManagedSourceWriteTime(std::filesystem::file_time_type& outWriteTime)
{
    if (!ResolvePieManagedCsprojPath())
    {
        return false;
    }

    const std::filesystem::path projectDir = g_pieManagedCsprojPath.parent_path();
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

static bool PreparePieManagedAutoPublishWorkspace(std::filesystem::path& outWorkspaceDir, std::string& outErrorMessage)
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

    const std::filesystem::path sourceDir = g_pieManagedCsprojPath.parent_path();
    outWorkspaceDir = workspaceRoot / (L"ws_" + std::to_wstring(++g_pieManagedAutoPublishGeneration));
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

static bool TryStartPieManagedAutoPublish()
{
    if (g_pieManagedPublishProcess != nullptr)
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
        g_pieGameStatus = "PIE auto-publish workspace failed: " + workspaceError;
        return false;
    }

    const std::filesystem::path projectDir = workspaceDir;
    g_pieManagedHotReloadPublishedDllPath = projectDir / L"out" / L"native" / L"PieGameManaged.dll";
    std::error_code ec;
    const std::filesystem::path logDir = std::filesystem::temp_directory_path(ec) / L"DirectX12Samples" / L"PieHotReload";
    if (ec)
    {
        g_pieGameStatus = "PIE auto-publish failed to resolve log directory";
        return false;
    }
    std::filesystem::create_directories(logDir, ec);
    if (ec)
    {
        g_pieGameStatus = "PIE auto-publish failed to create log directory";
        return false;
    }

    const std::filesystem::path logPath = logDir / L"pie_autopublish.log";
    g_pieManagedLastPublishLogPath = logPath.u8string();
    std::wstring commandLine =
        L"cmd.exe /C dotnet publish PieGameManaged.csproj -c Debug -r win-x64 --nologo /t:Rebuild -o out\\native > \"" +
        logPath.wstring() +
        L"\" 2>&1";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        projectDir.c_str(),
        &si,
        &pi))
    {
        g_pieGameStatus = "PIE auto-publish failed to start";
        return false;
    }

    CloseHandle(pi.hThread);
    g_pieManagedPublishProcess = pi.hProcess;
    g_pieGameStatus = "PIE auto-publish running...";
    return true;
}

static void TickPieManagedAutoPublish(float deltaTime)
{
    if (!g_isPieRunning)
    {
        return;
    }

    if (g_pieManagedPublishProcess != nullptr)
    {
        const DWORD waitResult = WaitForSingleObject(g_pieManagedPublishProcess, 0);
        if (waitResult == WAIT_OBJECT_0)
        {
            DWORD exitCode = 1;
            GetExitCodeProcess(g_pieManagedPublishProcess, &exitCode);
            CloseHandle(g_pieManagedPublishProcess);
            g_pieManagedPublishProcess = nullptr;
            if (exitCode == 0)
            {
                if (g_pieManagedPendingPublishSourceWriteTimeValid)
                {
                    g_pieManagedLastPublishedSourceWriteTime = g_pieManagedPendingPublishSourceWriteTime;
                    g_pieManagedLastPublishedSourceWriteTimeValid = true;
                }
                bool reloaded = false;
                const auto candidates = BuildPieManagedReloadCandidates();
                for (const auto& candidate : candidates)
                {
                    if (ReloadPieGameModuleFromPath(candidate, "PIE auto-publish completed + reloaded", "PIE auto-publish completed, but reload failed"))
                    {
                        reloaded = true;
                        break;
                    }
                }
                if (!reloaded)
                {
                    g_pieGameStatus = "PIE auto-publish completed, but no newer reload candidate was found";
                }
            }
            else
            {
                g_pieGameStatus = "PIE auto-publish failed (exit code=" + std::to_string(exitCode) + ")";
                if (!g_pieManagedLastPublishLogPath.empty())
                {
                    g_pieGameStatus += " log=" + g_pieManagedLastPublishLogPath;
                }
            }
            g_pieManagedPendingPublishSourceWriteTimeValid = false;
        }
        return;
    }

    g_pieManagedPublishCheckTimer += deltaTime;
    if (g_pieManagedPublishCheckTimer < 1.0f)
    {
        return;
    }
    g_pieManagedPublishCheckTimer = 0.0f;

    std::filesystem::file_time_type currentSourceWriteTime = {};
    if (!TryGetPieManagedSourceWriteTime(currentSourceWriteTime))
    {
        return;
    }

    if (!g_pieManagedLastPublishedSourceWriteTimeValid)
    {
        g_pieManagedLastPublishedSourceWriteTime = currentSourceWriteTime;
        g_pieManagedLastPublishedSourceWriteTimeValid = true;
        return;
    }

    if (currentSourceWriteTime <= g_pieManagedLastPublishedSourceWriteTime)
    {
        return;
    }

    if (TryStartPieManagedAutoPublish())
    {
        g_pieManagedPendingPublishSourceWriteTime = currentSourceWriteTime;
        g_pieManagedPendingPublishSourceWriteTimeValid = true;
    }
}

static std::vector<std::filesystem::path> BuildPieManagedReloadCandidates()
{
    std::vector<std::filesystem::path> candidates;
    if (!g_pieManagedHotReloadPublishedDllPath.empty())
    {
        candidates.push_back(g_pieManagedHotReloadPublishedDllPath);
    }

    if (ResolvePieManagedCsprojPath())
    {
        const auto projectDir = g_pieManagedCsprojPath.parent_path();
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

        if (g_pieGameSourceWriteTimeValid)
        {
            std::error_code ecTime;
            const auto candidateTime = std::filesystem::last_write_time(candidate, ecTime);
            if (ecTime)
            {
                continue;
            }
            // Only accept candidates that are newer than the currently loaded source module.
            if (candidateTime <= g_pieGameSourceWriteTime)
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

static bool EnsurePieGameModuleLoaded()
{
    if (g_pieGameModule != nullptr)
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

    g_pieGameStatus = "Invalid or missing C# game module. Need NativeAOT DLL exports: GameStart/GameTick/GameStop.";
    return false;
}

static void UnloadPieGameModule()
{
    if (g_pieGameModule != nullptr)
    {
        FreeLibrary(g_pieGameModule);
        g_pieGameModule = nullptr;
    }

    g_pieGameStart = nullptr;
    g_pieGameTick = nullptr;
    g_pieGameStop = nullptr;
    g_pieGameModulePath.clear();
    g_pieGameSourceModulePath.clear();
    g_pieGameSourceWriteTimeValid = false;
    g_pieGameLastLoadError.clear();
}

static bool TryHotReloadPieGameModule()
{
    if (g_pieGameModule == nullptr || g_pieGameSourceModulePath.empty() || !g_pieGameSourceWriteTimeValid)
    {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path sourcePath = std::filesystem::path(g_pieGameSourceModulePath);
    const auto currentWriteTime = std::filesystem::last_write_time(sourcePath, ec);
    if (ec)
    {
        return false;
    }
    if (currentWriteTime == g_pieGameSourceWriteTime)
    {
        return false;
    }

    return ReloadPieGameModuleNow("PIE hot reloaded (C#)", "PIE hot reload failed: unable to load updated C# module");
}

static bool ReloadPieGameModuleNow(const char* successStatus, const char* failureStatus)
{
    const bool wasRunning = g_isPieRunning;
    if (wasRunning && g_pieGameStop != nullptr)
    {
        g_pieGameStop();
    }
    DestroyAllGameQuads();
    g_isPieRunning = false;

    UnloadPieGameModule();
    if (!EnsurePieGameModuleLoaded())
    {
        g_pieGameStatus = failureStatus;
        return false;
    }

    if (wasRunning && g_pieGameStart != nullptr)
    {
        g_pieGameStart();
        g_isPieRunning = true;
    }

    g_pieGameStatus = successStatus;
    return true;
}

static bool ReloadPieGameModuleFromPath(const std::filesystem::path& modulePath, const char* successStatus, const char* failureStatus)
{
    const bool wasRunning = g_isPieRunning;
    HMODULE oldModule = g_pieGameModule;
    PieGameStopFn oldStop = g_pieGameStop;

    if (!TryLoadPieGameModuleAtPath(modulePath))
    {
        g_pieGameStatus = std::string(failureStatus) + " path=" + modulePath.u8string();
        if (!g_pieGameLastLoadError.empty())
        {
            g_pieGameStatus += " detail=" + g_pieGameLastLoadError;
        }
        return false;
    }

    if (wasRunning && oldStop != nullptr)
    {
        oldStop();
    }
    DestroyAllGameQuads();

    if (oldModule != nullptr)
    {
        FreeLibrary(oldModule);
    }

    if (wasRunning && g_pieGameStart != nullptr)
    {
        g_pieGameStart();
        g_isPieRunning = true;
    }
    else
    {
        g_isPieRunning = wasRunning;
    }

    g_pieGameStatus = successStatus;
    return true;
}

/// <summary>
/// ウィンドウの作成
/// </summary>
/// <returns></returns>
extern "C" __declspec(dllexport) HWND CreateNativeWindow()
{
    // コンソールをUTF-8モードに設定
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HINSTANCE hInstance = GetModuleHandle(NULL);

    LPCTSTR className = _T("NativeWindowClass");

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    RECT windowRect = { 0, 0, Application::GetWindowWidth(), Application::GetWindowHeight() };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindow(
        className,
        g_isStandaloneMode ? _T("PieGameManaged Player") : _T("Native Window"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL, NULL,
        hInstance,
        NULL
    );

	if (g_hwnd != NULL)
	{
        RECT clientRect = {};
        if (GetClientRect(g_hwnd, &clientRect))
        {
            Application::SetWindowSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            EditorUi::RequestSceneRenderSize(
                static_cast<UINT>((std::max)(1L, clientRect.right - clientRect.left)),
                static_cast<UINT>((std::max)(1L, clientRect.bottom - clientRect.top)));
        }

		g_DxDevice.Initialize(g_hwnd, Application::GetWindowWidth(), Application::GetWindowHeight());
        ConfigureD3D12DebugFilters();
        g_imguiInitialized = InitializeImGui();
       
		SceneManager::GetInstance().ChangeScene(0);
	}
    OutputDebugStringA("=== Main Loop START ===\n");
    return g_hwnd;
}

extern "C" __declspec(dllexport) void ShowNativeWindow()
{
    if (g_hwnd != NULL)
    {
        ShowWindow(g_hwnd, SW_SHOW);
        UpdateWindow(g_hwnd);
    }
}

extern "C" __declspec(dllexport) void HideNativeWindow()
{
    if (g_hwnd != NULL)
    {
        ShowWindow(g_hwnd, SW_HIDE);
    }
}

/// <summary>
/// ウィンドウの破棄
/// </summary>
extern "C" __declspec(dllexport) void DestroyNativeWindow()
{
    if (g_hwnd != NULL)
    {
        StopPieImmediate();
        ShutdownImGui();
        g_DxDevice.Shutdown();  // この関数を追加する
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
}

extern "C" __declspec(dllexport) void SetPieTickCallback(PieTickCallback callback)
{
    g_pieTickCallback = callback;
}

extern "C" __declspec(dllexport) void StartPie()
{
    g_pendingStopPie = false;
    g_pendingStartPie = true;
}

extern "C" __declspec(dllexport) void StopPie()
{
    g_pendingStartPie = false;
    g_pendingStopPie = true;
}

extern "C" __declspec(dllexport) void SetStandaloneMode(BOOL enabled)
{
    g_isStandaloneMode = (enabled == TRUE);
    if (g_hwnd != NULL)
    {
        SetWindowText(g_hwnd, g_isStandaloneMode ? _T("PieGameManaged Player") : _T("Native Window"));
    }
}

static void StartPieImmediate()
{
    if (!EnsurePieGameModuleLoaded())
    {
        g_isPieRunning = false;
        return;
    }

    const std::string statusBeforeStart = g_pieGameStatus;
    g_pieGameStart();
    g_isPieRunning = true;
    g_pieHotReloadCheckTimer = 0.0f;
    g_pieManagedPublishCheckTimer = 0.0f;
    g_pieManagedPendingPublishSourceWriteTimeValid = false;
    std::filesystem::file_time_type initialSourceWriteTime = {};
    if (TryGetPieManagedSourceWriteTime(initialSourceWriteTime))
    {
        g_pieManagedLastPublishedSourceWriteTime = initialSourceWriteTime;
        g_pieManagedLastPublishedSourceWriteTimeValid = true;
    }
    if (g_gameQuads.empty() && g_pieGameStatus == statusBeforeStart)
    {
        g_pieGameStatus = "PIE running (C#) - GameStart created no quads (no native error reported)";
        return;
    }
    if (g_pieGameStatus == statusBeforeStart)
    {
        g_pieGameStatus = "PIE running (C#)";
    }
}

static void StopPieImmediate()
{
    if (g_isPieRunning && g_pieGameStop != nullptr)
    {
        g_pieGameStop();
    }
    DestroyAllGameQuads();
    g_isPieRunning = false;
    g_pieHotReloadCheckTimer = 0.0f;
    g_pieManagedPublishCheckTimer = 0.0f;
    g_pieManagedPendingPublishSourceWriteTimeValid = false;
    if (g_pieManagedPublishProcess != nullptr)
    {
        CloseHandle(g_pieManagedPublishProcess);
        g_pieManagedPublishProcess = nullptr;
    }
    UnloadPieGameModule();
    g_pieGameStatus = "PIE stopped";
}

extern "C" __declspec(dllexport) BOOL IsPieRunning()
{
    return g_isPieRunning ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) void SetGameClearColor(float r, float g, float b, float a)
{
    g_gameClearColor[0] = r;
    g_gameClearColor[1] = g;
    g_gameClearColor[2] = b;
    g_gameClearColor[3] = a;
}

extern "C" __declspec(dllexport) uint32_t CreateGameQuad()
{
    try
    {
        const uint32_t handle = g_nextGameQuadHandle++;
        g_gameQuads[handle] = std::make_unique<PolygonTest>();
        g_pieGameStatus = "Game quad created. handle=" + std::to_string(handle);
        return handle;
    }
    catch (const std::exception& ex)
    {
        g_pieGameStatus = std::string("CreateGameQuad failed: ") + ex.what();
        return 0;
    }
    catch (...)
    {
        g_pieGameStatus = "CreateGameQuad failed: unknown error";
        return 0;
    }
}

extern "C" __declspec(dllexport) void DestroyGameQuad(uint32_t handle)
{
    if (handle == 0)
    {
        return;
    }

    g_gameQuads.erase(handle);
}

extern "C" __declspec(dllexport) void SetGameQuadTransform(uint32_t handle, float centerX, float centerY, float width, float height)
{
    const auto it = g_gameQuads.find(handle);
    if (it == g_gameQuads.end() || it->second == nullptr)
    {
        return;
    }

    it->second->SetTransform(centerX, centerY, width, height);
}


/// <summary>
/// メインループ
/// </summary>
extern "C" __declspec(dllexport) void MessageLoopIteration()
{
    if (g_hwnd == NULL)
    {
        return;
    }
    MSG msg = {};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // WM_DESTROY でシャットダウン済みなら同フレーム内の描画処理を中断する
    if (g_hwnd == NULL)
    {
        return;
    }

    if (g_pendingStopPie)
    {
        g_pendingStopPie = false;
        StopPieImmediate();
    }
    if (g_pendingStartPie)
    {
        g_pendingStartPie = false;
        StartPieImmediate();
    }

    constexpr float kFixedDeltaTime = 1.0f / 60.0f;

    SceneManager::GetInstance().Update(kFixedDeltaTime); // シーンの更新処理
    TickPieManagedAutoPublish(kFixedDeltaTime);
    if (g_isPieRunning)
    {
        g_pieHotReloadCheckTimer += kFixedDeltaTime;
        if (g_pieHotReloadCheckTimer >= 0.5f)
        {
            g_pieHotReloadCheckTimer = 0.0f;
            TryHotReloadPieGameModule();
        }
    }
    if (g_isPieRunning && g_pieGameTick != nullptr)
    {
        g_pieGameTick(kFixedDeltaTime);
    }
    if (g_isPieRunning && g_pieTickCallback != nullptr)
    {
        g_pieTickCallback(kFixedDeltaTime);
    }

    if (g_imguiInitialized)
    {
        EditorUi::EnsureSceneRenderSize();
    }

    BeginSceneRenderToTexture();
	SceneManager::GetInstance().Render();   // シーンのレンダリング処理
    RenderGameQuads();
    EndSceneRenderToTexture();

    g_DxDevice.PreRender();                 // DirectXのレンダー前更新処理

    if (g_imguiInitialized)
    {
        const std::string moduleSourceText = g_pieGameSourceModulePath.empty()
            ? (g_pieManagedHotReloadPublishedDllPath.empty() ? "(none)" : g_pieManagedHotReloadPublishedDllPath.u8string())
            : g_pieGameSourceModulePath;
        EditorUiRuntimeState uiState = {};
        uiState.isPieRunning = g_isPieRunning;
        uiState.pieGameStatus = g_pieGameStatus.c_str();
        uiState.pieGameLastLoadError = g_pieGameLastLoadError.c_str();
        uiState.moduleSourceText = moduleSourceText.c_str();
        uiState.pieGameModulePath = g_pieGameModulePath.c_str();
        uiState.pieManagedLastPublishLogPath = g_pieManagedLastPublishLogPath.c_str();
        uiState.activeQuadCount = static_cast<int>(g_gameQuads.size());

        EditorUiCallbacks uiCallbacks = {};
        uiCallbacks.startPie = &StartPie;
        uiCallbacks.stopPie = &StopPie;

        EditorUi::RenderFrame(g_isStandaloneMode, uiState, uiCallbacks);
    }

    g_DxDevice.Render();                    // DirectXの更新処理
}
