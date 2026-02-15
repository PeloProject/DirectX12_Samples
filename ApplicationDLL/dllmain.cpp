// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include "Source/DirectXDevice.h"
#include "SceneManager.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_dx12.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"


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
static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_imguiSrvHeap;
static constexpr UINT kFrameCount = 2;
static UINT g_imguiSrvDescriptorSize = 0;
static UINT g_imguiSrvCapacity = 64;
static UINT g_imguiSrvAllocated = 0;
using PieTickCallback = void(__cdecl*)(float);
static PieTickCallback g_pieTickCallback = nullptr;
static bool g_isPieRunning = false;
static Microsoft::WRL::ComPtr<ID3D12Resource> g_sceneRenderTarget;
static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_sceneRtvHeap;
static D3D12_CPU_DESCRIPTOR_HANDLE g_sceneRtvCpuHandle = {};
static D3D12_CPU_DESCRIPTOR_HANDLE g_sceneSrvCpuHandle = {};
static D3D12_GPU_DESCRIPTOR_HANDLE g_sceneSrvGpuHandle = {};
static bool g_sceneSrvAllocated = false;
static UINT g_sceneRenderWidth = 1;
static UINT g_sceneRenderHeight = 1;

static void ImGuiSrvDescriptorAlloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuDescHandle);
static void ImGuiSrvDescriptorFree(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle);
static bool CreateOrResizeSceneRenderTexture(UINT width, UINT height);
static void DestroySceneRenderTexture();
static void BeginSceneRenderToTexture();
static void EndSceneRenderToTexture();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool InitializeImGui();
static void ShutdownImGui();
static void RenderEditorDockingUi();

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SIZE && hwnd == g_hwnd)
    {
        const UINT width = LOWORD(lParam);
        const UINT height = HIWORD(lParam);

        if (wParam != SIZE_MINIMIZED && width > 0 && height > 0)
        {
            Application::SetWindowSize(static_cast<int>(width), static_cast<int>(height));
            g_DxDevice.Resize(width, height);
            if (g_imguiInitialized)
            {
                CreateOrResizeSceneRenderTexture(width, height);
            }
        }
        return 0;
    }

    if (g_imguiInitialized && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
    {
        return 1;
    }

    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        ShutdownImGui();
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
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static bool InitializeImGui()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = g_imguiSrvCapacity;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12Device* device = DirectXDevice::GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_imguiSrvHeap))))
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hwnd);
    g_imguiSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_imguiSrvAllocated = 0;

    ID3D12CommandQueue* commandQueue = g_DxDevice.GetCommandQueue();
    if (commandQueue == nullptr)
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiSrvHeap.Reset();
        return false;
    }

    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = device;
    initInfo.CommandQueue = commandQueue;
    initInfo.NumFramesInFlight = kFrameCount;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = g_imguiSrvHeap.Get();
    initInfo.SrvDescriptorAllocFn = ImGuiSrvDescriptorAlloc;
    initInfo.SrvDescriptorFreeFn = ImGuiSrvDescriptorFree;

    if (!ImGui_ImplDX12_Init(&initInfo))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiSrvHeap.Reset();
        return false;
    }

    CreateOrResizeSceneRenderTexture(static_cast<UINT>(Application::GetWindowWidth()), static_cast<UINT>(Application::GetWindowHeight()));

    return true;
}

static void ShutdownImGui()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroySceneRenderTexture();
    g_imguiSrvHeap.Reset();
    g_imguiSrvAllocated = 0;
    g_imguiInitialized = false;
}

static void ImGuiSrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuDescHandle)
{
    IM_ASSERT(g_imguiSrvAllocated < g_imguiSrvCapacity);
    const UINT descriptorIndex = g_imguiSrvAllocated++;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = g_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = g_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += static_cast<SIZE_T>(descriptorIndex) * g_imguiSrvDescriptorSize;
    gpuHandle.ptr += static_cast<UINT64>(descriptorIndex) * g_imguiSrvDescriptorSize;
    *outCpuDescHandle = cpuHandle;
    *outGpuDescHandle = gpuHandle;
}

static void ImGuiSrvDescriptorFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)
{
    // Minimal allocator: descriptors are reused on next context init.
}

static bool CreateOrResizeSceneRenderTexture(UINT width, UINT height)
{
    if (width == 0 || height == 0)
    {
        return false;
    }

    ID3D12Device* device = DirectXDevice::GetDevice();
    if (device == nullptr || g_imguiSrvHeap == nullptr)
    {
        return false;
    }

    if (g_sceneRenderTarget != nullptr && g_sceneRenderWidth == width && g_sceneRenderHeight == height)
    {
        return true;
    }

    g_sceneRenderTarget.Reset();
    g_sceneRtvHeap.Reset();

    if (!g_sceneSrvAllocated)
    {
        ImGuiSrvDescriptorAlloc(nullptr, &g_sceneSrvCpuHandle, &g_sceneSrvGpuHandle);
        g_sceneSrvAllocated = true;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_sceneRtvHeap))))
    {
        return false;
    }

    g_sceneRtvCpuHandle = g_sceneRtvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&g_sceneRenderTarget))))
    {
        return false;
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(g_sceneRenderTarget.Get(), &rtvDesc, g_sceneRtvCpuHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(g_sceneRenderTarget.Get(), &srvDesc, g_sceneSrvCpuHandle);

    g_sceneRenderWidth = width;
    g_sceneRenderHeight = height;
    return true;
}

static void DestroySceneRenderTexture()
{
    g_sceneRenderTarget.Reset();
    g_sceneRtvHeap.Reset();
    g_sceneRenderWidth = 1;
    g_sceneRenderHeight = 1;
}

static void BeginSceneRenderToTexture()
{
    if (g_sceneRenderTarget == nullptr)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = DirectXDevice::GetCommandList();
    if (commandList == nullptr)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_sceneRenderTarget.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList->ResourceBarrier(1, &barrier);

    commandList->OMSetRenderTargets(1, &g_sceneRtvCpuHandle, TRUE, nullptr);
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(g_sceneRtvCpuHandle, clearColor, 0, nullptr);
}

static void EndSceneRenderToTexture()
{
    if (g_sceneRenderTarget == nullptr)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = DirectXDevice::GetCommandList();
    if (commandList == nullptr)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_sceneRenderTarget.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);
}

static void RenderEditorDockingUi()
{
    ImGuiWindowFlags hostWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    hostWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    hostWindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("EditorDockSpaceHost", nullptr, hostWindowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New Level");
            ImGui::MenuItem("Save All");
            ImGui::Separator();
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("World Outliner");
            ImGui::MenuItem("Details");
            ImGui::MenuItem("Content Browser");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();

    ImGui::Begin("World Outliner");
    ImGui::Text("DemoActor");
    ImGui::BulletText("DirectionalLight");
    ImGui::BulletText("MainCamera");
    ImGui::BulletText("SM_Cube_01");
    ImGui::End();

    ImGui::Begin("Details");
    ImGui::Text("Transform");
    static float location[3] = { 0.0f, 0.0f, 0.0f };
    static float rotation[3] = { 0.0f, 0.0f, 0.0f };
    static float scale[3] = { 1.0f, 1.0f, 1.0f };
    ImGui::DragFloat3("Location", location, 0.1f);
    ImGui::DragFloat3("Rotation", rotation, 0.5f);
    ImGui::DragFloat3("Scale", scale, 0.01f);
    ImGui::End();

    ImGui::Begin("Content Browser");
    ImGui::Text("Assets");
    ImGui::Separator();
    ImGui::BulletText("Materials/M_Default");
    ImGui::BulletText("Meshes/SM_Cube");
    ImGui::BulletText("Textures/T_Checker");
    ImGui::End();

    ImGui::Begin("PIE Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Play In Editor");
    if (ImGui::Button(g_isPieRunning ? "Stop PIE" : "Start PIE"))
    {
        g_isPieRunning = !g_isPieRunning;
    }
    ImGui::End();

    ImGuiWindowFlags viewportWindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, viewportWindowFlags);
    ImGui::PopStyleColor();
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    if (availableSize.x > 1.0f && availableSize.y > 1.0f && g_sceneRenderTarget != nullptr)
    {
        ImTextureID sceneTextureId = (ImTextureID)(intptr_t)g_sceneSrvGpuHandle.ptr;
        ImGui::Image(sceneTextureId, availableSize);
    }
    else
    {
        ImGui::Text("Scene render target is not ready.");
    }
    ImGui::End();
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
        _T("Native Window"),
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
        }

		g_DxDevice.Initialize(g_hwnd, Application::GetWindowWidth(), Application::GetWindowHeight());
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
        g_isPieRunning = false;
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
    g_isPieRunning = true;
}

extern "C" __declspec(dllexport) void StopPie()
{
    g_isPieRunning = false;
}

extern "C" __declspec(dllexport) BOOL IsPieRunning()
{
    return g_isPieRunning ? TRUE : FALSE;
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

    SceneManager::GetInstance().Update(1.0f / 60.0f); // シーンの更新処理
    if (g_isPieRunning && g_pieTickCallback != nullptr)
    {
        g_pieTickCallback(1.0f / 60.0f);
    }

    BeginSceneRenderToTexture();
	SceneManager::GetInstance().Render();   // シーンのレンダリング処理
    EndSceneRenderToTexture();

    g_DxDevice.PreRender();                 // DirectXのレンダー前更新処理

    if (g_imguiInitialized)
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderEditorDockingUi();
        ImGui::Render();

        ID3D12DescriptorHeap* descriptorHeaps[] = { g_imguiSrvHeap.Get() };
        DirectXDevice::GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectXDevice::GetCommandList());
    }

    g_DxDevice.Render();                    // DirectXの更新処理
}
