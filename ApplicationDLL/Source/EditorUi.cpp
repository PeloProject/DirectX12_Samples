#include "pch.h"
#include "EditorUi.h"

#include "DirectXDevice.h"

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_dx12.h"
#include "ThirdParty/imgui/backends/imgui_impl_opengl2.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"

#include <algorithm>
#include <cmath>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    static bool g_initialized = false;
    static HWND g_hwnd = nullptr;
    static RendererBackend g_rendererBackend = RendererBackend::DirectX12;

    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_imguiSrvHeap;
    static constexpr UINT kFrameCount = 2;
    static UINT g_imguiSrvDescriptorSize = 0;
    static UINT g_imguiSrvCapacity = 64;
    static UINT g_imguiSrvAllocated = 0;

    static Microsoft::WRL::ComPtr<ID3D12Resource> g_sceneRenderTarget;
    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_sceneRtvHeap;
    static D3D12_CPU_DESCRIPTOR_HANDLE g_sceneRtvCpuHandle = {};
    static D3D12_CPU_DESCRIPTOR_HANDLE g_sceneSrvCpuHandle = {};
    static D3D12_GPU_DESCRIPTOR_HANDLE g_sceneSrvGpuHandle = {};
    static bool g_sceneSrvAllocated = false;
    static UINT g_sceneRenderWidth = 1;
    static UINT g_sceneRenderHeight = 1;
    static UINT g_sceneRequestedRenderWidth = 1;
    static UINT g_sceneRequestedRenderHeight = 1;

    static float g_sceneViewZoom = 1.0f;
    static ImVec2 g_sceneViewPan = ImVec2(0.0f, 0.0f);
    static float g_sceneViewZoom3D = 1.0f;
    static ImVec2 g_sceneViewPan3D = ImVec2(0.0f, 0.0f);
    static float g_sceneViewOrbitYawDeg = 45.0f;
    static float g_sceneViewOrbitPitchDeg = 35.0f;
    static bool g_sceneViewUse3DGrid = false;
    static constexpr float kSceneGridBaseSpacing = 32.0f;
    static bool g_rendererSwitchQueued = false;
    static uint32_t g_queuedRendererBackend = static_cast<uint32_t>(RendererBackend::DirectX12);
    static int g_rendererSwitchDelayFrames = 0;

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

        if (FAILED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
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
        g_sceneSrvAllocated = false;
        g_sceneSrvCpuHandle = {};
        g_sceneSrvGpuHandle = {};
        g_sceneRtvCpuHandle = {};
        g_sceneRenderWidth = 1;
        g_sceneRenderHeight = 1;
    }

    static void DrawSceneViewGrid(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        const float width = max.x - min.x;
        const float height = max.y - min.y;
        if (width <= 1.0f || height <= 1.0f)
        {
            return;
        }

        const ImU32 minorColor = IM_COL32(74, 74, 78, 128);
        const ImU32 majorColor = IM_COL32(112, 112, 118, 168);
        const ImU32 axisXColor = IM_COL32(210, 90, 90, 210);
        const ImU32 axisYColor = IM_COL32(90, 190, 210, 210);

        if (!g_sceneViewUse3DGrid)
        {
            const ImVec2 center = ImVec2(min.x + width * 0.5f, min.y + height * 0.5f);
            const ImVec2 origin = ImVec2(center.x + g_sceneViewPan.x, center.y + g_sceneViewPan.y);
            const float spacing = std::clamp(kSceneGridBaseSpacing * g_sceneViewZoom, 8.0f, 256.0f);
            const int majorStep = 5;

            auto calcStart = [](float rangeMin, float originCoord, float step) -> float
            {
                float start = rangeMin + fmodf(originCoord - rangeMin, step);
                if (start > rangeMin)
                {
                    start -= step;
                }
                return start;
            };

            const float startX = calcStart(min.x, origin.x, spacing);
            for (float x = startX; x <= max.x; x += spacing)
            {
                const int lineIndex = static_cast<int>(roundf((x - origin.x) / spacing));
                const bool isMajor = (lineIndex % majorStep) == 0;
                drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), isMajor ? majorColor : minorColor);
            }

            const float startY = calcStart(min.y, origin.y, spacing);
            for (float y = startY; y <= max.y; y += spacing)
            {
                const int lineIndex = static_cast<int>(roundf((y - origin.y) / spacing));
                const bool isMajor = (lineIndex % majorStep) == 0;
                drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), isMajor ? majorColor : minorColor);
            }

            if (origin.x >= min.x && origin.x <= max.x)
            {
                drawList->AddLine(ImVec2(origin.x, min.y), ImVec2(origin.x, max.y), axisYColor, 2.0f);
            }
            if (origin.y >= min.y && origin.y <= max.y)
            {
                drawList->AddLine(ImVec2(min.x, origin.y), ImVec2(max.x, origin.y), axisXColor, 2.0f);
            }
            return;
        }

        struct Vec3
        {
            float x;
            float y;
            float z;
        };

        auto sub = [](const Vec3& a, const Vec3& b) -> Vec3 { return Vec3{ a.x - b.x, a.y - b.y, a.z - b.z }; };
        auto dot = [](const Vec3& a, const Vec3& b) -> float { return a.x * b.x + a.y * b.y + a.z * b.z; };
        auto cross = [](const Vec3& a, const Vec3& b) -> Vec3
        {
            return Vec3{
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        };
        auto normalize = [](const Vec3& v) -> Vec3
        {
            const float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
            if (len <= 0.0001f)
            {
                return Vec3{ 0.0f, 0.0f, 0.0f };
            }
            return Vec3{ v.x / len, v.y / len, v.z / len };
        };

        const ImVec2 center = ImVec2(min.x + width * 0.5f, min.y + height * 0.5f);
        const float fovYRad = 60.0f * 3.14159265f / 180.0f;
        const float focal = (height * 0.5f) / tanf(fovYRad * 0.5f);
        const float yawRad = g_sceneViewOrbitYawDeg * 3.14159265f / 180.0f;
        const float pitchRad = g_sceneViewOrbitPitchDeg * 3.14159265f / 180.0f;
        const float distance = 28.0f / (std::max)(0.2f, g_sceneViewZoom3D);
        const float planeScale = 0.5f;

        const Vec3 target = Vec3{ g_sceneViewPan3D.x, 0.0f, g_sceneViewPan3D.y };
        const Vec3 camera = Vec3{
            target.x + distance * cosf(pitchRad) * cosf(yawRad),
            target.y + distance * sinf(pitchRad),
            target.z + distance * cosf(pitchRad) * sinf(yawRad)
        };

        const Vec3 forward = normalize(sub(target, camera));
        const Vec3 worldUp = Vec3{ 0.0f, 1.0f, 0.0f };
        const Vec3 right = normalize(cross(worldUp, forward));
        const Vec3 up = cross(forward, right);

        auto project = [&](const Vec3& p, ImVec2& out) -> bool
        {
            const Vec3 rel = sub(p, camera);
            const float viewX = dot(rel, right);
            const float viewY = dot(rel, up);
            const float viewZ = dot(rel, forward);
            if (viewZ <= 0.05f)
            {
                return false;
            }
            out.x = center.x + (viewX * focal / viewZ);
            out.y = center.y - (viewY * focal / viewZ);
            return true;
        };

        const int halfLineCount = 40;
        const int majorStep = 5;
        for (int i = -halfLineCount; i <= halfLineCount; ++i)
        {
            const float s = static_cast<float>(i) * planeScale;
            const bool isMajor = (i % majorStep) == 0;
            const ImU32 lineColor = isMajor ? majorColor : minorColor;

            ImVec2 a;
            ImVec2 b;
            if (project(Vec3{ -halfLineCount * planeScale, 0.0f, s }, a) &&
                project(Vec3{ halfLineCount * planeScale, 0.0f, s }, b))
            {
                drawList->AddLine(a, b, lineColor);
            }

            if (project(Vec3{ s, 0.0f, -halfLineCount * planeScale }, a) &&
                project(Vec3{ s, 0.0f, halfLineCount * planeScale }, b))
            {
                drawList->AddLine(a, b, lineColor);
            }
        }

        ImVec2 xAxisA;
        ImVec2 xAxisB;
        if (project(Vec3{ -halfLineCount * planeScale, 0.0f, 0.0f }, xAxisA) &&
            project(Vec3{ halfLineCount * planeScale, 0.0f, 0.0f }, xAxisB))
        {
            drawList->AddLine(xAxisA, xAxisB, axisXColor, 2.0f);
        }

        ImVec2 zAxisA;
        ImVec2 zAxisB;
        if (project(Vec3{ 0.0f, 0.0f, -halfLineCount * planeScale }, zAxisA) &&
            project(Vec3{ 0.0f, 0.0f, halfLineCount * planeScale }, zAxisB))
        {
            drawList->AddLine(zAxisA, zAxisB, axisYColor, 2.0f);
        }
    }

    static void RenderEditorDockingUi(const EditorUiRuntimeState& state, const EditorUiCallbacks& callbacks)
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
                if (ImGui::MenuItem("Exit") && g_hwnd != NULL)
                {
                    PostMessage(g_hwnd, WM_CLOSE, 0, 0);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem("World Outliner");
                ImGui::MenuItem("Details");
                ImGui::MenuItem("Content Browser");
                ImGui::MenuItem("Viewport");
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
        if (ImGui::Button(state.isPieRunning ? "Stop PIE" : "Start PIE"))
        {
            if (state.isPieRunning)
            {
                if (callbacks.stopPie != nullptr)
                {
                    callbacks.stopPie();
                }
            }
            else
            {
                if (callbacks.startPie != nullptr)
                {
                    callbacks.startPie();
                }
            }
        }
        ImGui::TextWrapped("%s", state.pieGameStatus != nullptr ? state.pieGameStatus : "");
        ImGui::Text("Renderer:");
        ImGui::SameLine();
        const bool isDx12 = state.currentRendererBackend == static_cast<uint32_t>(RendererBackend::DirectX12);
        const bool isVulkan = state.currentRendererBackend == static_cast<uint32_t>(RendererBackend::Vulkan);
        const bool isOpenGL = state.currentRendererBackend == static_cast<uint32_t>(RendererBackend::OpenGL);
        if (isDx12) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
        if (ImGui::Button("DirectX12"))
        {
            g_queuedRendererBackend = static_cast<uint32_t>(RendererBackend::DirectX12);
            g_rendererSwitchQueued = true;
            g_rendererSwitchDelayFrames = 1;
        }
        if (isDx12) { ImGui::PopStyleColor(); }
        ImGui::SameLine();
        if (isVulkan) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
        if (ImGui::Button("Vulkan"))
        {
            g_queuedRendererBackend = static_cast<uint32_t>(RendererBackend::Vulkan);
            g_rendererSwitchQueued = true;
            g_rendererSwitchDelayFrames = 1;
        }
        if (isVulkan) { ImGui::PopStyleColor(); }
        ImGui::SameLine();
        if (isOpenGL) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
        if (ImGui::Button("OpenGL"))
        {
            g_queuedRendererBackend = static_cast<uint32_t>(RendererBackend::OpenGL);
            g_rendererSwitchQueued = true;
            g_rendererSwitchDelayFrames = 1;
        }
        if (isOpenGL) { ImGui::PopStyleColor(); }

        if (g_rendererSwitchQueued)
        {
            if (g_rendererSwitchDelayFrames > 0)
            {
                --g_rendererSwitchDelayFrames;
            }
            else
            {
                if (callbacks.setRendererBackend != nullptr)
                {
                    callbacks.setRendererBackend(g_queuedRendererBackend);
                }
                g_rendererSwitchQueued = false;
            }
        }
        const std::string lastLoadErrorText = (state.pieGameLastLoadError == nullptr || state.pieGameLastLoadError[0] == '\0') ? "(none)" : state.pieGameLastLoadError;
        const std::string moduleSourceText = (state.moduleSourceText == nullptr || state.moduleSourceText[0] == '\0') ? "(none)" : state.moduleSourceText;
        ImGui::Text("Last load error (selectable):");
        static char lastLoadErrorBuffer[2048] = {};
        strncpy_s(lastLoadErrorBuffer, sizeof(lastLoadErrorBuffer), lastLoadErrorText.c_str(), _TRUNCATE);
        ImGui::InputTextMultiline("##LastLoadErrorSelectable", lastLoadErrorBuffer, sizeof(lastLoadErrorBuffer), ImVec2(600.0f, 48.0f), ImGuiInputTextFlags_ReadOnly);
        ImGui::Text("Module source (selectable):");
        static char moduleSourceBuffer[2048] = {};
        strncpy_s(moduleSourceBuffer, sizeof(moduleSourceBuffer), moduleSourceText.c_str(), _TRUNCATE);
        ImGui::InputTextMultiline("##ModuleSourceSelectable", moduleSourceBuffer, sizeof(moduleSourceBuffer), ImVec2(600.0f, 48.0f), ImGuiInputTextFlags_ReadOnly);
        ImGui::TextWrapped("Module loaded: %s", (state.pieGameModulePath == nullptr || state.pieGameModulePath[0] == '\0') ? "(none)" : state.pieGameModulePath);
        ImGui::TextWrapped("Publish log: %s", (state.pieManagedLastPublishLogPath == nullptr || state.pieManagedLastPublishLogPath[0] == '\0') ? "(none)" : state.pieManagedLastPublishLogPath);
        ImGui::Text("Active quads: %d", state.activeQuadCount);
        ImGui::End();

        ImGuiWindowFlags viewportWindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::Begin("Viewport", nullptr, viewportWindowFlags);
        if (ImGui::BeginTabBar("##ViewportTabs"))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                if (ImGui::Button(g_sceneViewUse3DGrid ? "Switch to 2D Grid" : "Switch to 3D Grid"))
                {
                    g_sceneViewUse3DGrid = !g_sceneViewUse3DGrid;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(g_sceneViewUse3DGrid ? "Grid: 3D" : "Grid: 2D");

                ImVec2 availableSize = ImGui::GetContentRegionAvail();
                if (availableSize.x > 1.0f && availableSize.y > 1.0f)
                {
                    g_sceneRequestedRenderWidth = static_cast<UINT>(availableSize.x);
                    g_sceneRequestedRenderHeight = static_cast<UINT>(availableSize.y);

                    ImVec2 canvasMin = ImGui::GetCursorScreenPos();
                    ImVec2 canvasMax = ImVec2(canvasMin.x + availableSize.x, canvasMin.y + availableSize.y);

                    ImGui::InvisibleButton("##SceneCanvas", availableSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
                    const bool isHovered = ImGui::IsItemHovered();

                    if (isHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
                    {
                        const ImVec2 delta = ImGui::GetIO().MouseDelta;
                        if (g_sceneViewUse3DGrid)
                        {
                            const float panScale3D = 0.02f / (std::max)(0.2f, g_sceneViewZoom3D);
                            const float yawRad = g_sceneViewOrbitYawDeg * 3.14159265f / 180.0f;
                            const float forwardX = -cosf(yawRad);
                            const float forwardZ = -sinf(yawRad);
                            const float rightX = forwardZ;
                            const float rightZ = -forwardX;
                            g_sceneViewPan3D.x += (-delta.x * rightX + delta.y * forwardX) * panScale3D;
                            g_sceneViewPan3D.y += (-delta.x * rightZ + delta.y * forwardZ) * panScale3D;
                        }
                        else
                        {
                            g_sceneViewPan.x += delta.x;
                            g_sceneViewPan.y += delta.y;
                        }
                    }

                    if (isHovered && g_sceneViewUse3DGrid && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
                    {
                        const ImVec2 delta = ImGui::GetIO().MouseDelta;
                        const float orbitSensitivity = 0.25f;
                        g_sceneViewOrbitYawDeg -= delta.x * orbitSensitivity;
                        g_sceneViewOrbitPitchDeg = std::clamp(g_sceneViewOrbitPitchDeg - delta.y * orbitSensitivity, 5.0f, 85.0f);
                    }

                    if (isHovered)
                    {
                        const float mouseWheel = ImGui::GetIO().MouseWheel;
                        if (mouseWheel != 0.0f)
                        {
                            if (g_sceneViewUse3DGrid)
                            {
                                g_sceneViewZoom3D = std::clamp(g_sceneViewZoom3D * (1.0f + mouseWheel * 0.1f), 0.2f, 4.0f);
                            }
                            else
                            {
                                const float previousZoom = g_sceneViewZoom;
                                g_sceneViewZoom = std::clamp(g_sceneViewZoom * (1.0f + mouseWheel * 0.1f), 0.2f, 4.0f);

                                const ImVec2 viewCenter = ImVec2((canvasMin.x + canvasMax.x) * 0.5f, (canvasMin.y + canvasMax.y) * 0.5f);
                                const ImVec2 mousePosition = ImGui::GetIO().MousePos;
                                const ImVec2 originBefore = ImVec2(viewCenter.x + g_sceneViewPan.x, viewCenter.y + g_sceneViewPan.y);
                                const ImVec2 worldFromMouse = ImVec2(
                                    (mousePosition.x - originBefore.x) / previousZoom,
                                    (mousePosition.y - originBefore.y) / previousZoom);
                                const ImVec2 originAfter = ImVec2(
                                    mousePosition.x - worldFromMouse.x * g_sceneViewZoom,
                                    mousePosition.y - worldFromMouse.y * g_sceneViewZoom);
                                g_sceneViewPan.x = originAfter.x - viewCenter.x;
                                g_sceneViewPan.y = originAfter.y - viewCenter.y;
                            }
                        }
                    }

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(canvasMin, canvasMax, true);
                    if (g_sceneRenderTarget != nullptr)
                    {
                        ImTextureID sceneTextureId = (ImTextureID)(intptr_t)g_sceneSrvGpuHandle.ptr;
                        drawList->AddImage(sceneTextureId, canvasMin, canvasMax);
                    }
                    else
                    {
                        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(12, 12, 14, 255));
                    }
                    DrawSceneViewGrid(drawList, canvasMin, canvasMax);
                    const char* hintText = g_sceneViewUse3DGrid
                        ? "Scene View  Grid: 3D  RMB: Orbit  MMB: Pan  Wheel: Zoom"
                        : "Scene View  Grid: 2D  MMB: Pan  Wheel: Zoom";
                    drawList->AddText(ImVec2(canvasMin.x + 10.0f, canvasMin.y + 10.0f), IM_COL32(210, 210, 210, 255), hintText);
                    drawList->PopClipRect();
                }
                else
                {
                    ImGui::Text("Scene render target is not ready.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Game"))
            {
                ImVec2 availableSize = ImGui::GetContentRegionAvail();
                if (availableSize.x > 1.0f && availableSize.y > 1.0f)
                {
                    g_sceneRequestedRenderWidth = static_cast<UINT>(availableSize.x);
                    g_sceneRequestedRenderHeight = static_cast<UINT>(availableSize.y);

                    ImVec2 canvasMin = ImGui::GetCursorScreenPos();
                    ImVec2 canvasMax = ImVec2(canvasMin.x + availableSize.x, canvasMin.y + availableSize.y);
                    ImGui::InvisibleButton("##GameCanvas", availableSize, ImGuiButtonFlags_None);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(canvasMin, canvasMax, true);
                    if (g_sceneRenderTarget != nullptr)
                    {
                        ImTextureID sceneTextureId = (ImTextureID)(intptr_t)g_sceneSrvGpuHandle.ptr;
                        drawList->AddImage(sceneTextureId, canvasMin, canvasMax);
                    }
                    else
                    {
                        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(12, 12, 14, 255));
                        drawList->AddText(ImVec2(canvasMin.x + 10.0f, canvasMin.y + 10.0f), IM_COL32(210, 210, 210, 255), "Game render target is not ready.");
                    }
                    drawList->PopClipRect();
                }
                else
                {
                    ImGui::Text("Game render target is not ready.");
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    static void RenderStandaloneViewportUi()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav;
        ImGui::Begin("StandaloneViewport", nullptr, flags);
        ImGui::PopStyleVar();

        if (g_sceneRenderTarget != nullptr)
        {
            ImVec2 availableSize = ImGui::GetContentRegionAvail();
            if (availableSize.x > 1.0f && availableSize.y > 1.0f)
            {
                g_sceneRequestedRenderWidth = static_cast<UINT>(availableSize.x);
                g_sceneRequestedRenderHeight = static_cast<UINT>(availableSize.y);
            }
            ImTextureID sceneTextureId = (ImTextureID)(intptr_t)g_sceneSrvGpuHandle.ptr;
            ImGui::Image(sceneTextureId, ImGui::GetContentRegionAvail());
        }
        else
        {
            ImGui::Text("Scene render target is not ready.");
        }

        ImGui::End();
    }
}

namespace EditorUi
{
    bool Initialize(RendererBackend backend, HWND hwnd, ID3D12CommandQueue* commandQueue, UINT initialWidth, UINT initialHeight)
    {
        g_rendererBackend = backend;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(hwnd);

        if (backend == RendererBackend::DirectX12)
        {
            ID3D12Device* device = DirectXDevice::GetDevice();
            if (device == nullptr || commandQueue == nullptr)
            {
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return false;
            }

            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.NumDescriptors = g_imguiSrvCapacity;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_imguiSrvHeap))))
            {
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return false;
            }

            g_imguiSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            g_imguiSrvAllocated = 0;

            ImGui_ImplDX12_InitInfo initInfo = {};
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
        }
        else
        {
            if (!ImGui_ImplOpenGL2_Init())
            {
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return false;
            }
        }

        g_hwnd = hwnd;
        g_sceneRequestedRenderWidth = initialWidth;
        g_sceneRequestedRenderHeight = initialHeight;
        if (backend == RendererBackend::DirectX12)
        {
            CreateOrResizeSceneRenderTexture(initialWidth, initialHeight);
        }
        g_initialized = true;
        return true;
    }

    void Shutdown()
    {
        if (!g_initialized)
        {
            return;
        }

        if (g_rendererBackend == RendererBackend::DirectX12)
        {
            ImGui_ImplDX12_Shutdown();
        }
        else
        {
            ImGui_ImplOpenGL2_Shutdown();
        }
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        DestroySceneRenderTexture();
        g_imguiSrvHeap.Reset();
        g_imguiSrvAllocated = 0;
        g_hwnd = nullptr;
        g_initialized = false;
    }

    bool IsInitialized()
    {
        return g_initialized;
    }

    bool HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (!g_initialized)
        {
            return false;
        }

        const bool handled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
        {
            ImGuiIO& io = ImGui::GetIO();
            LOG_DEBUG("EditorUi::HandleWndProc msg=%u handled=%d io.MouseDown[0]=%d io.WantCaptureMouse=%d",
                msg,
                handled ? 1 : 0,
                io.MouseDown[0] ? 1 : 0,
                io.WantCaptureMouse ? 1 : 0);
        }
        return handled;
    }

    bool WantsMouseCapture()
    {
        if (!g_initialized)
        {
            return false;
        }
        return ImGui::GetIO().WantCaptureMouse;
    }

    void InjectWin32Input(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        IM_UNUSED(msg);
        IM_UNUSED(wParam);
        IM_UNUSED(lParam);
    }

    void ResetInputState()
    {
        if (!g_initialized)
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseButtonEvent(0, false);
        io.AddMouseButtonEvent(1, false);
        io.AddMouseButtonEvent(2, false);
        io.AddMouseButtonEvent(3, false);
        io.AddMouseButtonEvent(4, false);

        POINT cursorPos = {};
        if (g_hwnd != nullptr && GetCursorPos(&cursorPos) && ScreenToClient(g_hwnd, &cursorPos))
        {
            io.AddMousePosEvent(static_cast<float>(cursorPos.x), static_cast<float>(cursorPos.y));
        }
    }

    void RequestSceneRenderSize(UINT width, UINT height)
    {
        if (width > 0 && height > 0)
        {
            g_sceneRequestedRenderWidth = width;
            g_sceneRequestedRenderHeight = height;
        }
    }

    bool EnsureSceneRenderSize()
    {
        if (!g_initialized || g_sceneRequestedRenderWidth == 0 || g_sceneRequestedRenderHeight == 0)
        {
            return false;
        }
        if (g_rendererBackend != RendererBackend::DirectX12)
        {
            return true;
        }
        return CreateOrResizeSceneRenderTexture(g_sceneRequestedRenderWidth, g_sceneRequestedRenderHeight);
    }

    void BeginSceneRenderToTexture(bool isPieRunning, const float* gameClearColor, const float* defaultClearColor)
    {
        if (g_rendererBackend != RendererBackend::DirectX12 || g_sceneRenderTarget == nullptr)
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
        const float* clearColor = isPieRunning ? gameClearColor : defaultClearColor;
        commandList->ClearRenderTargetView(g_sceneRtvCpuHandle, clearColor, 0, nullptr);
    }

    void EndSceneRenderToTexture()
    {
        if (g_rendererBackend != RendererBackend::DirectX12 || g_sceneRenderTarget == nullptr)
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

    void RenderFrame(bool isStandaloneMode, const EditorUiRuntimeState& state, const EditorUiCallbacks& callbacks)
    {
        if (!g_initialized)
        {
            return;
        }

        if (g_rendererBackend == RendererBackend::DirectX12)
        {
            ImGui_ImplDX12_NewFrame();
        }
        else
        {
            ImGui_ImplOpenGL2_NewFrame();
        }
        ImGui_ImplWin32_NewFrame();

        if (g_rendererBackend != RendererBackend::DirectX12)
        {
            ImGuiIO& io = ImGui::GetIO();
            POINT cursorPos = {};
            if (g_hwnd != nullptr && GetCursorPos(&cursorPos) && ScreenToClient(g_hwnd, &cursorPos))
            {
                io.AddMousePosEvent(static_cast<float>(cursorPos.x), static_cast<float>(cursorPos.y));
            }
            else
            {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            }

            io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
            io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
            io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
        }

        if (g_rendererBackend != RendererBackend::DirectX12)
        {
            ImGui::GetIO().DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        }
        ImGui::NewFrame();

        if (isStandaloneMode)
        {
            RenderStandaloneViewportUi();
        }
        else
        {
            RenderEditorDockingUi(state, callbacks);
        }

        ImGui::Render();
        if (g_rendererBackend == RendererBackend::DirectX12)
        {
            ID3D12GraphicsCommandList* commandList = DirectXDevice::GetCommandList();
            if (commandList == nullptr)
            {
                return;
            }

            ID3D12DescriptorHeap* descriptorHeaps[] = { g_imguiSrvHeap.Get() };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
        }
        else
        {
            static uint32_t nonDxFrameCounter = 0;
            ++nonDxFrameCounter;
            if ((nonDxFrameCounter % 240) == 0)
            {
                ImDrawData* drawData = ImGui::GetDrawData();
                const int cmdLists = (drawData != nullptr) ? drawData->CmdListsCount : -1;
                const int totalVtx = (drawData != nullptr) ? drawData->TotalVtxCount : -1;
                ImGuiIO& io = ImGui::GetIO();
                const ImVec2 displaySize = (drawData != nullptr) ? drawData->DisplaySize : ImVec2(0.0f, 0.0f);
                const ImVec2 fbScale = (drawData != nullptr) ? drawData->FramebufferScale : ImVec2(0.0f, 0.0f);
                LOG_DEBUG("EditorUi OpenGL/Vulkan frame: cmdLists=%d totalVtx=%d mousePos=(%.1f,%.1f) wantCapture=%d display=(%.1f,%.1f) fbScale=(%.1f,%.1f)",
                    cmdLists,
                    totalVtx,
                    io.MousePos.x,
                    io.MousePos.y,
                    io.WantCaptureMouse ? 1 : 0,
                    displaySize.x,
                    displaySize.y,
                    fbScale.x,
                    fbScale.y);
            }
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        }
    }
}
