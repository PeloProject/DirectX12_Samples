# DirectX12_Samples コード関係図

## 1. ソリューション全体のプロジェクト関係

```mermaid
graph TD
    SLN["DirectX12_Samples.sln"]
    DX12["DirectX12_Samples (DirectX12サンプルexe)"]
    CLS["CommandListSimulation (コマンドリスト実験exe)"]
    DLL["ApplicationDLL (ネイティブ中核DLL)"]
    HOST["ApplicationDLLHost (ネイティブホストexe)"]
    EDT["Editor (WPFエディタexe)"]
    PIE["PieGameManaged (CSharp NativeAOTゲームDLL)"]

    SLN --> DX12
    SLN --> CLS
    SLN --> DLL
    SLN --> HOST
    SLN --> EDT
    SLN --> PIE

    HOST --> DLL
    EDT --> DLL
    DLL --> PIE
    PIE --> DLL
```

## 2. ランタイムの呼び出しフロー（ApplicationDLL経路）

```mermaid
sequenceDiagram
    participant Host as ApplicationDLLHost または Editor
    participant Dll as ApplicationDLL.dll
    participant Core as AppRuntime::MessageLoopIteration
    participant UI as EditorUi (ImGui)
    participant Scene as SceneManager
    participant Pie as PieGameManaged.dll

    Host->>Dll: CreateNativeWindow()
    loop 毎フレーム
        Host->>Dll: MessageLoopIteration()
        Dll->>Core: フレーム更新
        Core->>Scene: Update/Render
        Core->>Pie: GameTick(delta) ※PIE実行中
        Core->>UI: RenderFrame(...)
    end
    Host->>Dll: DestroyNativeWindow()
```

## 3. ApplicationDLL 内部モジュール関係

```mermaid
graph TD
    RT["dllmain.cpp / AppRuntime"]
    DX["DirectXDevice"]
    EU["EditorUi"]
    SM["SceneManager"]
    SG["SceneGame"]
    PT["PolygonTest"]
    SH["Shader hlsl"]
    PM["PieGameManaged module loader"]

    RT --> DX
    RT --> EU
    RT --> SM
    RT --> PM
    RT --> PT

    SM --> SG
    EU --> DX
    PT --> DX
    PT --> SH
```

## 4. 関数レベル詳細図

### 4.1 ApplicationDLL のエクスポートAPI

```mermaid
graph TD
    EXP["ApplicationDLL exports"]
    C1["CreateNativeWindow"]
    C2["ShowNativeWindow"]
    C3["HideNativeWindow"]
    C4["DestroyNativeWindow"]
    C5["MessageLoopIteration"]
    C6["SetPieTickCallback"]
    C7["StartPie"]
    C8["StopPie"]
    C9["SetStandaloneMode"]
    C10["IsPieRunning"]
    C11["SetGameClearColor"]
    C12["CreateGameQuad"]
    C13["DestroyGameQuad"]
    C14["SetGameQuadTransform"]

    EXP --> C1
    EXP --> C2
    EXP --> C3
    EXP --> C4
    EXP --> C5
    EXP --> C6
    EXP --> C7
    EXP --> C8
    EXP --> C9
    EXP --> C10
    EXP --> C11
    EXP --> C12
    EXP --> C13
    EXP --> C14
```

### 4.2 ApplicationDLLHost 側の関数呼び出し順

```mermaid
sequenceDiagram
    participant Host as ApplicationDLLHost::wWinMain
    participant DLL as ApplicationDLL.dll

    Host->>DLL: LoadLibraryW("ApplicationDLL.dll")
    Host->>DLL: GetProcAddress(...)
    Host->>DLL: SetStandaloneMode(TRUE) ※--game時
    Host->>DLL: CreateNativeWindow()
    Host->>DLL: ShowNativeWindow()
    Host->>DLL: StartPie() ※--game時
    loop IsWindow(hwnd)
        Host->>DLL: MessageLoopIteration()
    end
    Host->>DLL: StopPie() ※--game時
    Host->>DLL: DestroyNativeWindow()
```

### 4.3 Editor 側の関数呼び出し関係

```mermaid
graph TD
    T["DispatcherTimer.Tick"]
    ML["NativeInterop.MessageLoopIteration"]
    IR["NativeInterop.IsPieRunning"]
    SH["pieGameHost.Start"]
    ST["pieGameHost.Stop"]
    CB["OnPieTickFromNative"]
    GT["pieGameHost.Tick"]
    SC["NativeInterop.SetPieTickCallback"]

    T --> ML
    T --> IR
    IR --> SH
    IR --> ST
    SC --> CB
    CB --> GT
```

### 4.4 MessageLoopIteration の内部処理

```mermaid
flowchart TD
    A["MessageLoopIteration"] --> B["Win32メッセージ処理"]
    B --> C["Start/Stop PIE の遅延要求を反映"]
    C --> D["SceneManager.Update(delta)"]
    D --> E["TickPieManagedAutoPublish(delta)"]
    E --> F["TryHotReloadPieGameModule()"]
    F --> G["g_pieGameTick(delta)"]
    G --> H["g_pieTickCallback(delta)"]
    H --> I["EditorUi.EnsureSceneRenderSize()"]
    I --> J["BeginSceneRenderToTexture()"]
    J --> K["SceneManager.Render()"]
    K --> L["RenderGameQuads()"]
    L --> M["EndSceneRenderToTexture()"]
    M --> N["DirectXDevice.PreRender()"]
    N --> O["EditorUi.RenderFrame(...)"]
    O --> P["DirectXDevice.Render()"]
```

### 4.5 PIE（CSharp NativeAOT）双方向API

```mermaid
graph LR
    DLL["ApplicationDLL"]
    CSHARP["PieGameManaged.GameEntry"]
    GS["GameStart"]
    GT["GameTick"]
    GE["GameStop"]
    N1["SetGameClearColor"]
    N2["CreateGameQuad"]
    N3["DestroyGameQuad"]
    N4["SetGameQuadTransform"]

    DLL --> GS
    DLL --> GT
    DLL --> GE

    GS --> N1
    GS --> N2
    GT --> N1
    GT --> N4
    GE --> N1
    GE --> N3

    CSHARP --> GS
    CSHARP --> GT
    CSHARP --> GE
```

## 5. 現在コードから読み取れる要点

- `Editor` と `ApplicationDLLHost` はどちらも `ApplicationDLL` のフロントエンドで、実フレームループは `ApplicationDLL` 側にあります。
- `ApplicationDLL` は制御API（`CreateNativeWindow`、`MessageLoopIteration`、`StartPie`、`StopPie` など）を公開します。
- PIE は `PieGameManaged.dll` を動的ロードして `GameStart` / `GameTick` / `GameStop` を呼び出し、ホットリロードにも対応しています。
- `DirectX12_Samples` と `CommandListSimulation` は独立サンプルであり、`ApplicationDLL` 実行経路には直接参加しません。

## 6. クラス図

### 6.1 ApplicationDLL 中心

```mermaid
classDiagram
    class AppRuntime {
      +HWND CreateNativeWindow()
      +void ShowNativeWindow()
      +void HideNativeWindow()
      +void DestroyNativeWindow()
      +void SetPieTickCallback(PieTickCallback)
      +void RequestStartPie()
      +void RequestStopPie()
      +BOOL IsPieRunning()
      +void MessageLoopIteration()
    }

    class DirectXDevice {
      +bool Initialize(HWND, UINT, UINT)
      +void PreRender()
      +void Render()
      +bool Resize(UINT, UINT)
      +void Shutdown()
      +ID3D12CommandQueue* GetCommandQueue()
      +static ID3D12Device* GetDevice()
      +static ID3D12GraphicsCommandList* GetCommandList()
    }

    class SceneManager {
      +void ChangeScene(int)
      +void Update(float)
      +void Render()
    }

    class SceneBase {
      <<abstract>>
      +Update(float)*
      +Render()*
    }

    class SceneGame {
      +SceneGame()
      +Update(float)
      +Render()
    }

    class PolygonTest {
      +PolygonTest()
      +void SetTransform(float, float, float, float)
      +void Render()
    }

    class Application {
      +static int GetWindowWidth()
      +static int GetWindowHeight()
      +static void SetWindowSize(int, int)
    }

    AppRuntime --> DirectXDevice : 所有
    AppRuntime --> SceneManager : 呼び出し
    AppRuntime --> PolygonTest : 管理
    SceneManager --> SceneBase : 保持
    SceneBase <|-- SceneGame : 継承
    PolygonTest --> DirectXDevice : 利用
    PolygonTest --> Application : 利用
```

### 6.2 Editor / PIE 連携

```mermaid
classDiagram
    class MainWindow {
      -IntPtr nativeHwnd
      -DispatcherTimer messageLoopTimer
      +BtnCreate_Click(...)
      +BtnDestroy_Click(...)
      +MessageLoopTimer_Tick(...)
    }

    class NativeInterop {
      <<static>>
      +CreateNativeWindow()
      +DestroyNativeWindow()
      +MessageLoopIteration()
      +SetPieTickCallback(...)
      +IsPieRunning()
    }

    class PieGameHost {
      -bool isRunning
      +Start()
      +Tick(float)
      +Stop()
    }

    class IPieGame {
      <<interface>>
      +Start()
      +Tick(float)
      +Stop()
    }

    class SamplePieGame {
      +Start()
      +Tick(float)
      +Stop()
    }

    class GameEntry {
      <<static>>
      +GameStart()
      +GameTick(float)
      +GameStop()
    }

    MainWindow --> NativeInterop : P/Invoke呼び出し
    MainWindow --> PieGameHost : 所有
    PieGameHost --> IPieGame : 依存
    IPieGame <|.. SamplePieGame : 実装
    GameEntry --> NativeInterop : DllImport(ApplicationDLL)
```
