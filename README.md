# DirectX12_Samples

## PieGameManaged executable launch

`ApplicationDLLHost.exe` now supports a game mode that starts PIE automatically and shows only the game viewport.

- Editor mode (existing): `ApplicationDLLHost.exe`
- Game mode (new): `ApplicationDLLHost.exe --game`

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
