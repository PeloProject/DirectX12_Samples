// dllmain.cpp : DLL exports entry points.
#include "pch.h"
#include "AppRuntime.h"

//BOOL APIENTRY DllMain(HMODULE hModule,
//                      DWORD  ul_reason_for_call,
//                      LPVOID lpReserved)
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

extern "C" __declspec(dllexport) HWND CreateNativeWindow()
{
    return Runtime().CreateNativeWindow();
}

extern "C" __declspec(dllexport) void ShowNativeWindow()
{
    Runtime().ShowNativeWindow();
}

extern "C" __declspec(dllexport) void HideNativeWindow()
{
    Runtime().HideNativeWindow();
}

extern "C" __declspec(dllexport) void DestroyNativeWindow()
{
    Runtime().DestroyNativeWindow();
}

extern "C" __declspec(dllexport) void SetPieTickCallback(PieTickCallback callback)
{
    Runtime().SetPieTickCallback(callback);
}

extern "C" __declspec(dllexport) void StartPie()
{
    Runtime().RequestStartPie();
}

extern "C" __declspec(dllexport) void StopPie()
{
    Runtime().RequestStopPie();
}

extern "C" __declspec(dllexport) void SetStandaloneMode(BOOL enabled)
{
    Runtime().SetStandaloneMode(enabled);
}

extern "C" __declspec(dllexport) BOOL IsPieRunning()
{
    return Runtime().IsPieRunning();
}

extern "C" __declspec(dllexport) void SetGameClearColor(float r, float g, float b, float a)
{
    Runtime().SetGameClearColor(r, g, b, a);
}

extern "C" __declspec(dllexport) uint32_t CreateGameQuad()
{
    return Runtime().CreateGameQuad();
}

extern "C" __declspec(dllexport) void DestroyGameQuad(uint32_t handle)
{
    Runtime().DestroyGameQuad(handle);
}

extern "C" __declspec(dllexport) void SetGameQuadTransform(uint32_t handle, float centerX, float centerY, float width, float height)
{
    Runtime().SetGameQuadTransform(handle, centerX, centerY, width, height);
}

extern "C" __declspec(dllexport) void MessageLoopIteration()
{
    Runtime().MessageLoopIteration();
}

extern "C" __declspec(dllexport) BOOL SetRendererBackend(uint32_t backend)
{
    return Runtime().SetRendererBackend(backend);
}

extern "C" __declspec(dllexport) uint32_t GetRendererBackend()
{
    return Runtime().GetRendererBackend();
}
