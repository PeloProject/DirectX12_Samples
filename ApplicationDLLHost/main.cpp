#include <windows.h>

using CreateNativeWindowFn = HWND(__cdecl*)();
using ShowNativeWindowFn = void(__cdecl*)();
using DestroyNativeWindowFn = void(__cdecl*)();
using MessageLoopIterationFn = void(__cdecl*)();

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    HMODULE module = LoadLibraryW(L"ApplicationDLL.dll");
    if (module == nullptr)
    {
        MessageBoxW(nullptr, L"Failed to load ApplicationDLL.dll.", L"ApplicationDLLHost", MB_ICONERROR | MB_OK);
        return -1;
    }

    auto createNativeWindow = reinterpret_cast<CreateNativeWindowFn>(GetProcAddress(module, "CreateNativeWindow"));
    auto showNativeWindow = reinterpret_cast<ShowNativeWindowFn>(GetProcAddress(module, "ShowNativeWindow"));
    auto destroyNativeWindow = reinterpret_cast<DestroyNativeWindowFn>(GetProcAddress(module, "DestroyNativeWindow"));
    auto messageLoopIteration = reinterpret_cast<MessageLoopIterationFn>(GetProcAddress(module, "MessageLoopIteration"));

    if (createNativeWindow == nullptr || showNativeWindow == nullptr || destroyNativeWindow == nullptr || messageLoopIteration == nullptr)
    {
        MessageBoxW(nullptr, L"Failed to resolve required exports from ApplicationDLL.dll.", L"ApplicationDLLHost", MB_ICONERROR | MB_OK);
        FreeLibrary(module);
        return -2;
    }

    HWND hwnd = createNativeWindow();
    if (hwnd == nullptr)
    {
        MessageBoxW(nullptr, L"CreateNativeWindow failed.", L"ApplicationDLLHost", MB_ICONERROR | MB_OK);
        FreeLibrary(module);
        return -3;
    }

    showNativeWindow();

    while (IsWindow(hwnd))
    {
        messageLoopIteration();
        Sleep(16);
    }

    destroyNativeWindow();
    FreeLibrary(module);
    return 0;
}
