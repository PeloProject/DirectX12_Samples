#include <windows.h>
#include <cwctype>

using CreateNativeWindowFn = HWND(__cdecl*)();
using ShowNativeWindowFn = void(__cdecl*)();
using DestroyNativeWindowFn = void(__cdecl*)();
using MessageLoopIterationFn = void(__cdecl*)();
using GetNativeWindowHandleFn = HWND(__cdecl*)();
using StartPieFn = void(__cdecl*)();
using StopPieFn = void(__cdecl*)();
using SetStandaloneModeFn = void(__cdecl*)(BOOL);

static bool HasGameModeArg()
{
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine == nullptr)
    {
        return false;
    }

    const wchar_t* p = commandLine;
    while (*p != L'\0')
    {
        while (*p != L'\0' && iswspace(*p))
        {
            ++p;
        }
        if (*p == L'\0')
        {
            break;
        }

        const wchar_t* tokenStart = p;
        while (*p != L'\0' && !iswspace(*p))
        {
            ++p;
        }
        const size_t tokenLength = static_cast<size_t>(p - tokenStart);
        if (tokenLength == 6 && wcsncmp(tokenStart, L"--game", 6) == 0)
        {
            return true;
        }

        if (tokenLength == 5 &&
            (wcsncmp(tokenStart, L"-game", 5) == 0 || wcsncmp(tokenStart, L"/game", 5) == 0))
        {
            return true;
        }
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const bool gameMode = HasGameModeArg();

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
    auto getNativeWindowHandle = reinterpret_cast<GetNativeWindowHandleFn>(GetProcAddress(module, "GetNativeWindowHandle"));
    auto startPie = reinterpret_cast<StartPieFn>(GetProcAddress(module, "StartPie"));
    auto stopPie = reinterpret_cast<StopPieFn>(GetProcAddress(module, "StopPie"));
    auto setStandaloneMode = reinterpret_cast<SetStandaloneModeFn>(GetProcAddress(module, "SetStandaloneMode"));

    if (createNativeWindow == nullptr || showNativeWindow == nullptr || destroyNativeWindow == nullptr || messageLoopIteration == nullptr)
    {
        MessageBoxW(nullptr, L"Failed to resolve required exports from ApplicationDLL.dll.", L"ApplicationDLLHost", MB_ICONERROR | MB_OK);
        FreeLibrary(module);
        return -2;
    }

    if (gameMode && startPie == nullptr)
    {
        MessageBoxW(nullptr, L"Game mode requires StartPie export.", L"ApplicationDLLHost", MB_ICONERROR | MB_OK);
        FreeLibrary(module);
        return -4;
    }

    if (gameMode && setStandaloneMode != nullptr)
    {
        setStandaloneMode(TRUE);
    }

    HWND hwnd = createNativeWindow();
    if (hwnd == nullptr)
    {
        MessageBoxW(nullptr, L"CreateNativeWindow failed.", L"ApplicationDLLHost", MB_ICONERROR | MB_OK);
        FreeLibrary(module);
        return -3;
    }

    showNativeWindow();

    if (gameMode)
    {
        startPie();
    }

    while (true)
    {
        messageLoopIteration();
        HWND activeHwnd = (getNativeWindowHandle != nullptr) ? getNativeWindowHandle() : hwnd;
        if (activeHwnd == nullptr || !IsWindow(activeHwnd))
        {
            break;
        }
        Sleep(16);
    }

    if (gameMode && stopPie != nullptr)
    {
        stopPie();
    }

    destroyNativeWindow();
    // Process shutdown will unload the DLL. Explicit FreeLibrary here can block in teardown.
    return 0;
}
