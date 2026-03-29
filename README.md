# DirectX12_Samples

## Editor frontends

`Editor.exe` is now the single entry point for editor and standalone launch modes. The former WPF `Editor` and `ApplicationDLLHost` projects have been removed from the solution.

Build requirements:

- Qt 6 Widgets SDK
- CMake 3.21+
- `CMAKE_PREFIX_PATH` pointing to your Qt installation, for example `C:\Qt\6.8.0\msvc2022_64`

Build example:

```powershell
cd EditorQt
cmake --preset default
cmake --build --preset default
```

Launch modes:

- `Editor.exe`
- `Editor.exe --ui=qt`
- `Editor.exe --ui=imgui`
- `Editor.exe --game`

Design notes:

- Qt is the default editor frontend and hosts the tool chrome with dock widgets.
- ImGui remains available as an optional editor frontend via `--ui=imgui`.
- `--game` bypasses editor UI entirely and runs the standalone viewport loop from `Editor.exe`.
- `ApplicationDLL` can run with its internal ImGui editor UI enabled or disabled; the runtime is driven through exported DLL functions either way.
- The current viewport embedding is still Win32-specific, but the frontend boundary is now explicit so another UI library can be added on the `Editor` side.

Required files in the same folder:

- `ApplicationDLL.dll`
- `PieGameManaged.dll` (NativeAOT output)
## Optional: GLAD integration for OpenGL backend

`ApplicationDLL` now supports GLAD if the following files are present:

- `ApplicationDLL/ThirdParty/glad/include/glad/glad.h`
- `ApplicationDLL/ThirdParty/glad/src/glad.c`

When these files exist, `OpenGLRenderDevice` initializes GLAD automatically.  
When they do not exist, the project falls back to system OpenGL headers (`gl/GL.h`).

`ApplicationDLL.vcxproj` already has a conditional compile entry for `glad.c`, so no extra project edit is required.
