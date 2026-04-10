# Editor Qt 設計・コード読解ガイド

このドキュメントは `EditorQt` の Qt フロントエンド実装を、学習用に「どこが入口で、何が責務で、どこまでが Qt の仕事か」を追いやすい形で整理したものです。

対象読者は、次のような意図を持ってコードを読む人です。

- Qt 版 Editor の全体設計を把握したい
- ImGui 版との違いを理解したい
- `ApplicationDLL` と Qt UI の責務分離を理解したい
- Docking System やネイティブウィンドウ埋め込みの実装を追いたい
- 将来的にパネル追加や機能拡張をしたい

## 1. まず結論: この Qt Editor は何者か

この `EditorQt` は、レンダリングやゲーム実行を自前で持つアプリではありません。

役割は次の 2 層に分かれています。

- `ApplicationDLL`
  - 実際のネイティブウィンドウ生成
  - Renderer 初期化
  - PIE 実行
  - Scene 描画
  - 既存 ImGui Editor UI の保持
- `EditorQt`
  - Qt のアプリケーションライフサイクル管理
  - Dock レイアウト
  - Scene / Game のネイティブウィンドウを Qt に埋め込む
  - Outliner / Details / Content Browser / Log などの外側 UI を提供

つまり、Qt 版 Editor は「エンジン本体を DLL 越しに操作する薄いフロントエンド」です。

## 2. 全体アーキテクチャ

概念図:

```text
+-----------------------------------------------------------+
| Editor.exe (EditorQt)                                     |
|                                                           |
|  main.cpp                                                 |
|    -> LaunchOptions                                       |
|    -> RuntimeBridge                                       |
|    -> QtEditorFrontend / ImGuiEditorFrontend              |
|                                                           |
|  QtEditorFrontend                                         |
|    -> MainWindow                                          |
|         -> ads::CDockManager                              |
|         -> NativeViewportHost(Scene)                      |
|         -> NativeViewportHost(Game)                       |
|         -> Outliner / Details / Content / Log             |
|                                                           |
+-------------------------|---------------------------------+
                          |
                          | LoadLibrary + exported C API
                          v
+-----------------------------------------------------------+
| ApplicationDLL.dll                                        |
|                                                           |
|  AppRuntime                                               |
|    -> CreateNativeWindow / DestroyNativeWindow            |
|    -> Renderer lifetime                                   |
|    -> PIE control                                         |
|    -> MessageLoopIteration                                |
|    -> Renderer backend switching                          |
|    -> runtime status / error text                         |
|                                                           |
+-----------------------------------------------------------+
```

Qt 側は `RuntimeBridge` を通じて DLL の C export を呼び出します。  
Qt 側が Renderer や Scene を直接所有するわけではありません。

## 3. エントリポイントと起動フロー

主要ファイル:

- `EditorQt/src/main.cpp`
- `EditorQt/src/LaunchOptions.h`
- `EditorQt/src/LaunchOptions.cpp`
- `EditorQt/src/EditorRuntimeHost.h`
- `EditorQt/src/EditorRuntimeHost.cpp`

### 3.1 `main.cpp` の流れ

起動時の大まかな流れは以下です。

1. `ParseLaunchOptions(argc, argv)` でモードを決める
2. `GetEditorBaseDirectory()` で実行ファイル基準のディレクトリを取る
3. `RuntimeBridge runtime;` を作る
4. `--game` なら Qt を使わず standalone game loop に入る
5. それ以外なら `ApplicationDLL.dll` をロードする
6. `--ui=imgui` なら `ImGuiEditorFrontend`
7. それ以外は既定で `QtEditorFrontend`

### 3.2 起動オプション

`LaunchOptions` は非常に小さく、今のところ次だけを管理します。

- `--game`
  - Qt Editor ではなく単独ゲーム起動
- `--ui=imgui`
  - 既存 ImGui UI を使う
- `--ui=qt`
  - Qt UI を使う

ここからわかる通り、この実装は「Qt 版を追加した」が本質であって、「既存実装を完全置換した」構造ではありません。  
フロントエンドは差し替え可能です。

## 4. フロントエンド抽象化

主要ファイル:

- `EditorQt/src/IEditorFrontend.h`
- `EditorQt/src/ImGuiEditorFrontend.h`
- `EditorQt/src/ImGuiEditorFrontend.cpp`
- `EditorQt/src/QtEditorFrontend.h`
- `EditorQt/src/QtEditorFrontend.cpp`

### 4.1 `IEditorFrontend`

`IEditorFrontend` は次の責務だけを持つ、非常に薄い抽象です。

- `bindRuntimeBridge`
- `initialize`
- `run`
- `shutdown`
- `requestClose`
- `showError`

つまり「UI バックエンドを差し替えるための最小インターフェース」です。

### 4.2 ImGui 版との違い

`ImGuiEditorFrontend` は単純です。

- `ApplicationDLL` に対して `setStandaloneMode(false)`
- `createNativeWindow(true)` で DLL 側 UI を有効化
- 自前ループで `tick()` を 16ms ごとに回す

ImGui 版では DLL のネイティブウィンドウ自体が Editor UI を持っています。

一方で `QtEditorFrontend` は次の流れです。

- `QApplication` を受け取る
- `MainWindow` を生成する
- `MainWindow::initialize(runtime_, baseDir)` を呼ぶ
- 実行ループは `app_.exec()`

ここで大事なのは、Qt 版は DLL 側 UI を見せないことです。

- `sceneRuntime_->setEditorUiEnabled(false)`
- `gameRuntime_->setEditorUiEnabled(false)`

つまり、描画とゲーム制御は DLL 側、Editor の外枠 UI は Qt 側に寄せています。

## 5. `QtEditorFrontend` の責務

主要ファイル:

- `EditorQt/src/QtEditorFrontend.h`
- `EditorQt/src/QtEditorFrontend.cpp`

`QtEditorFrontend` は薄いオーケストレータです。

- `QApplication` の参照を持つ
- `RuntimeBridge*` を保持
- `MainWindow` の生成と破棄を担当
- `run()` で Qt event loop を回す

`initialize()` の本質は以下です。

1. `runtime_` が束縛済みか確認
2. `MainWindow` を `new`
3. `MainWindow::initialize(runtime_, baseDir)` を呼ぶ
4. 成功したら `show()`

ロジックの大半は `MainWindow` にあります。  
そのため、Qt 実装を読むときの中心ファイルは `MainWindow.cpp` です。

## 6. `RuntimeBridge`: Qt と `ApplicationDLL` の境界

主要ファイル:

- `EditorQt/src/RuntimeBridge.h`
- `EditorQt/src/RuntimeBridge.cpp`
- `ApplicationDLL/dllmain.cpp`

### 6.1 役割

`RuntimeBridge` は Qt/C++ コードから DLL export を安全に呼ぶためのラッパです。

公開メソッドは次の 3 系統に分かれます。

- DLL ライフサイクル
  - `load`
  - `isLoaded`
  - `unload` 相当は private + destructor
- ネイティブウィンドウ制御
  - `createNativeWindow`
  - `destroyNativeWindow`
  - `showNativeWindow`
  - `hideNativeWindow`
  - `nativeWindowHandle`
  - `isNativeWindowValid`
- 実行制御
  - `tick`
  - `startPie`
  - `stopPie`
  - `setStandaloneMode`
  - `setEditorUiEnabled`
  - `setRendererBackend`
  - `runtimeStatus`
  - `runtimeLastError`

### 6.2 なぜ DLL をコピーしてロードするのか

`RuntimeBridge::load(baseDir, instanceTag)` では、`instanceTag` が指定されると

- 元 DLL: `ApplicationDLL.dll`
- コピー DLL: `ApplicationDLL.<tag>.dll`

を作ってからロードします。

Qt 版 `MainWindow` では

- Scene 用: 元の `RuntimeBridge`
- Game 用: `RuntimeBridge::load(baseDir, "qt_game")`

という 2 インスタンス構成です。

これは、Scene Viewport と Game Viewport を別 DLL インスタンスとして持つためです。  
同一 DLL ハンドルを共有せず、別モジュールとしてロードすることで、実質的に別ランタイムを並列に持っています。

この設計は Qt 版の重要ポイントです。

### 6.3 解決している export

`RuntimeBridge::resolve()` で主に次の export を取っています。

- `CreateNativeWindow`
- `ShowNativeWindow`
- `HideNativeWindow`
- `DestroyNativeWindow`
- `MessageLoopIteration`
- `StartPie`
- `StopPie`
- `SetEditorUiEnabled`
- `SetStandaloneMode`
- `IsPieRunning`
- `SetRendererBackend`
- `GetRendererBackend`
- `GetRuntimeStatusText`
- `GetRuntimeLastErrorText`
- `GetNativeWindowHandle`

Qt 側はこの C API だけに依存しており、`ApplicationDLL` の内部実装詳細には基本的に踏み込みません。

## 7. `MainWindow` が実質的な Qt Editor 本体

主要ファイル:

- `EditorQt/src/MainWindow.h`
- `EditorQt/src/MainWindow.cpp`

`MainWindow` はこの Editor の中心です。責務はかなり広いですが、性質は明快です。

- 画面構成を作る
- 2 つの Runtime を初期化する
- Native window を Qt の widget に埋め込む
- Qt の操作を Runtime 操作へ変換する
- Editor 用のダミーデータを表示する
- レイアウト永続化を行う

### 7.1 主なメンバ

- Runtime 関係
  - `RuntimeBridge* sceneRuntime_`
  - `std::unique_ptr<RuntimeBridge> gameRuntime_`
- データモデル
  - `std::vector<EditorWorldActor> worldActors_`
- Dock 管理
  - `ads::CDockManager* dockManager_`
  - `ads::CDockWidget* ...`
- UI 部品
  - `NativeViewportHost* sceneViewportHost_`
  - `NativeViewportHost* gameViewportHost_`
  - `QListWidget* outlinerList_`
  - `AssetBrowserListWidget* contentList_`
  - `QPlainTextEdit* logView_`
  - `QDoubleSpinBox* locationSpin_[3]`
  - `QDoubleSpinBox* rotationSpin_[3]`
  - `QDoubleSpinBox* scaleSpin_[3]`
- 更新ループ
  - `QTimer* tickTimer_`

### 7.2 `initialize()` の流れ

`MainWindow::initialize()` の重要な処理順は次の通りです。

1. `sceneRuntime_` を保持
2. `initializeSecondaryRuntime(baseDir)` で `gameRuntime_` を作る
3. Scene Runtime を editor mode で起動
   - `setStandaloneMode(false)`
   - `setEditorUiEnabled(false)`
   - `createNativeWindow(false)`
4. Game Runtime を standalone mode で起動
   - `setStandaloneMode(true)`
   - `setEditorUiEnabled(false)`
   - `createNativeWindow(false)`
5. 両ネイティブウィンドウを `showNativeWindow()`
6. `attachSceneViewport()`, `attachGameViewport()`
7. `tickTimer_->start()`

この時点で、Qt 内部に 2 つのネイティブ描画ウィンドウが見える状態になります。

## 8. Scene と Game の 2 Runtime 構成

Qt 版設計の核はここです。

### 8.1 Scene Runtime

- `sceneRuntime_`
- `setStandaloneMode(false)`
- Editor 上の Scene 表示用

### 8.2 Game Runtime

- `gameRuntime_`
- `setStandaloneMode(true)`
- `qt_game` タグ付き DLL コピーをロード
- PIE 表示用

### 8.3 なぜ 2 つ必要か

1 つのネイティブウィンドウだけでは、Scene と Game を同時に独立表示できません。  
Qt の Dock 内で「Scene」と「Game」を別タブ、別フロートウィンドウ、別配置にしたいため、ランタイム自体を分けています。

これにより、

- Scene は編集ビュー
- Game は PIE ビュー

として並列表示できます。

制約もあります。

- 状態共有は自動ではない
- `worldActors_` は Qt 側のローカル表現であり、DLL の Scene と同期しているわけではない
- Scene/Game の厳密なエンジン状態共有まではまだ設計されていない

つまり現状は、完成したレベルエディタというより「Qt shell 上に 2 runtime を並べた学習・試作実装」と読むのが正確です。

## 9. Native ウィンドウ埋め込みの仕組み

主要クラス:

- `NativeViewportHost`

### 9.1 何をしているか

`NativeViewportHost` は Qt widget の中に Win32 の子ウィンドウをぶら下げるための受け皿です。

重要な設定:

- `setAttribute(Qt::WA_NativeWindow, true)`
- `setAttribute(Qt::WA_PaintOnScreen, true)`

`syncNativeWindow()` の中では Win32 API を使って、

- `SetParent(hwnd_, parentHwnd)`
- `SetWindowLongPtr(..., WS_CHILD | WS_VISIBLE | ...)`
- `MoveWindow(...)`
- `ShowWindow(hwnd_, SW_SHOW)`

を行っています。

つまり Qt がレンダリング結果を自前描画しているのではなく、DLL 側が作った Win32 window を Qt widget の子に付け替えています。

### 9.2 いつ同期するか

次のタイミングで再配置します。

- `setNativeWindow()`
- `resizeEvent()`
- `showEvent()`

Dock の移動、サイズ変更、フロート化に追従するにはこの再同期が必要です。

## 10. Docking System の構成

主要依存:

- `ThirdParty/Qt-Advanced-Docking-System`
- `ads::CDockManager`
- `ads::CDockWidget`

### 10.1 採用理由

Qt 標準の `QDockWidget` よりも、Visual Studio 風の使い勝手に近い機能を持たせたい意図が見えます。

有効化している代表的な機能:

- `OpaqueSplitterResize`
- `XmlCompressionEnabled = false`
- `FocusHighlighting`
- `EqualSplitOnInsertion`
- `FloatingContainerHasWidgetTitle`
- Auto Hide 一式

`buildUi()` で全体を構成し、Dock は `createDockWidget()` で共通生成しています。

### 10.2 現在のレイアウト

初期配置は概ね以下です。

```text
Top:    Tools
Center: Scene / Game (tab)
Left:   Scene Hierarchy
Right:  Details
Bottom: Content Browser + Log
```

Scene と Game はタブで同じ dock area に入り、そこからフロートや再配置ができます。

### 10.3 Auto Hide

`buildMenu()` では各 Dock に対して Auto Hide 用メニューを追加しています。  
Dock 本体の `toggleAutoHide(side)` とメニューのチェック状態を連動させています。

この構造は今後パネルを増やす際にも流用しやすいです。

## 11. UI パネルごとの責務

### 11.1 Tools

主な操作:

- `Create`
- `Destroy`
- `Show`
- `Hide`
- `Play`
- `Stop`
- Renderer backend 切り替え

これらはすべて `RuntimeBridge` 呼び出しへ落ちます。

### 11.2 Scene / Game Viewport

- `NativeViewportHost` の上にネイティブ描画 window を載せる
- Scene 側のみ asset drag & drop を受ける

### 11.3 Scene Hierarchy

- `worldActors_` の名前一覧を表示
- 現在は Qt 側ローカルデータ

### 11.4 Details

- 選択中 Actor の transform を編集
- 値変更は `worldActors_` を更新するだけ
- 実際の `ApplicationDLL` scene graph へはまだ送っていない

### 11.5 Content Browser

- サンプル asset 一覧
- `AssetBrowserListWidget::mimeData()` で custom MIME を作る

### 11.6 Log

- `appendLogMessage()` で `[Info]` / `[Error]` を追加
- Runtime 状態や UI 操作の学習ログとして有効

## 12. UI データモデルはまだ仮実装

`EditorWorldActor` は次のような純粋 UI モデルです。

- `actorName`
- `sourceAssetPath`
- `location[3]`
- `rotation[3]`
- `scale[3]`

重要なのは、これは現時点でエンジン内実体ではないことです。

`spawnActorFromAssetPath()` を見ると分かる通り、

- asset path から名前を作る
- `worldActors_` に push
- outliner / details を更新

という UI ローカル操作です。

つまり今の Editor は、

- 見た目は Editor らしい
- Runtime 制御は本物
- Actor/Details の多くはデモデータ

という段階です。

学習時にはここを誤解しない方がよいです。

## 13. Tick モデル

Qt 版は `QTimer` を 16ms 間隔で回し、その中で

- `sceneRuntime_->tick()`
- `gameRuntime_->tick()`
- `updateStatus()`

を呼んでいます。

つまりメインループは Qt event loop に委譲し、エンジン更新はポーリングで混ぜています。

ImGui 版のような

```text
while (window valid) {
  tick();
  Sleep(16);
}
```

とは異なり、Qt 版は GUI アプリの通常構造に従っています。

### 13.1 この設計の意味

メリット:

- Qt のイベント処理と自然に共存できる
- Dock 移動や UI 入力が扱いやすい
- ネイティブウィンドウを外側 UI に統合しやすい

注意点:

- Tick と UI 更新が同一スレッド前提
- 重い処理は Qt 操作感を悪化させる
- 将来の async asset scan や scene sync には別設計が要る

## 14. `ApplicationDLL` 側で Qt 版に効いているポイント

主要ファイル:

- `ApplicationDLL/dllmain.cpp`
- `ApplicationDLL/AppRuntime.h`
- `ApplicationDLL/WindowHost.cpp`
- `ApplicationDLL/FrameLoop.cpp`

### 14.1 `SetEditorUiEnabled(false)` の意味

`WindowHost.cpp` では `g_editorUiEnabled` が false の場合、ImGui 初期化を行いません。

結果として Qt 版では、

- Renderer は動く
- ネイティブ window は作る
- DLL 内 Editor UI は出さない

という状態になります。

これが Qt 版成立の前提です。

### 14.2 `SetStandaloneMode`

`g_isStandaloneMode` はウィンドウタイトルや PIE 挙動に影響します。  
Qt 版では Scene と Game でこの値を分け、役割を分離しています。

### 14.3 `GetRuntimeStatusText`

Qt の `updateStatus()` は DLL 側文字列を読んでウィンドウタイトルやログに反映します。  
そのため、Runtime 状態表示の真実は DLL 側にあります。

## 15. レイアウト永続化

主要処理:

- `restoreLayout()`
- `saveLayout()`

利用しているもの:

- `QSettings`
- `saveGeometry()`
- `dockManager_->saveState(kLayoutVersion)`

保存キー:

- `MainWindow/Geometry`
- `MainWindow/AdsState`

これにより、

- 前回のウィンドウサイズ
- Dock の配置
- フロート状態

が復元されます。

Qt 学習の観点では、「Dock 構成を組み立てる処理」と「永続化」は別責務としてきれいに分けられています。

## 16. ビルド構成

主要ファイル:

- `EditorQt/Editor.vcxproj`
- `EditorQt/CMakeLists.txt`

### 16.1 実際に使っているのはどちらか

現状の主系統は `Editor.vcxproj` です。  
理由は以下です。

- Qt include/lib が明示設定されている
- `Qt-Advanced-Docking-System` の `src/*.cpp` を直接ビルド対象に含めている
- `generated/ads/*.cpp` も含めている
- `ApplicationDLL` プロジェクト参照がある
- `windeployqt` を post build で実行している

一方 `CMakeLists.txt` はかなり最小で、現在の実装全体を反映していません。  
学習・保守対象としては MSBuild 側を正と見るのが安全です。

### 16.2 `ADS_STATIC`

`Editor.vcxproj` の preprocessor definitions に `ADS_STATIC` が入っています。  
つまり Docking System は別 DLL ではなく、Editor 実行ファイルへ静的に取り込む前提です。

### 16.3 出力先

`OutDir` は `..\Editor\bin\...\` です。  
つまり Qt フロントエンドの成果物は既存 `Editor` 実行配置へ寄せています。

これは既存ランタイムや managed DLL との共存を意識した配置です。

## 17. コードを読む順番

最短で理解したいなら、以下の順に読むのが効率的です。

1. `EditorQt/src/main.cpp`
2. `EditorQt/src/IEditorFrontend.h`
3. `EditorQt/src/QtEditorFrontend.cpp`
4. `EditorQt/src/MainWindow.h`
5. `EditorQt/src/MainWindow.cpp`
6. `EditorQt/src/RuntimeBridge.cpp`
7. `ApplicationDLL/dllmain.cpp`
8. `ApplicationDLL/WindowHost.cpp`
9. `ApplicationDLL/FrameLoop.cpp`

この順だと、

- 起動
- 抽象化
- Qt frontend
- MainWindow
- DLL bridge
- DLL export
- 実際の runtime

という依存方向に沿って追えます。

## 18. 重要な設計判断

### 18.1 Qt は「外側 UI」に限定している

Qt で scene を再描画しているわけではなく、ネイティブ window を埋め込んでいます。  
これは、既存 Renderer を大きく壊さずに Editor UX を改善するための現実的な設計です。

### 18.2 Runtime は DLL C API で切っている

C++ クラスを直接共有せず export 関数にしているため、

- UI 側変更の影響範囲を狭めやすい
- ImGui / Qt の切り替えがしやすい
- 将来的に別 frontend を増やしやすい

という利点があります。

### 18.3 2 Runtime 構成は強いが、同期問題を抱える

Scene と Game を独立表示できる一方で、

- scene data 同期
- selection 同期
- asset instantiate の実体化
- editor command undo/redo

などはまだ未整備です。

このため、今後本格的な Editor にするなら「Qt 表示モデル」と「engine authoritative state」の接続設計が必要になります。

## 19. 今後拡張するときの見どころ

### 19.1 新しい Dock パネルを足す

やることは基本的に次の通りです。

1. `buildUi()` で widget を作る
2. `createDockWidget()` で dock 化する
3. `dockManager_->addDockWidget(...)` で配置する
4. `buildMenu()` に toggle / auto hide を足す

### 19.2 Runtime と本当に同期した Details にする

今は `worldActors_` がローカルです。  
本当に Editor にするなら、

- DLL export を追加
- actor list 取得 API を作る
- transform 更新 API を作る
- Qt 側で選択変更時に runtime を更新する

という経路が必要です。

### 19.3 Asset Browser を本物にする

今は `kSampleAssets` の固定配列です。  
将来的には

- filesystem scan
- asset database
- thumbnail
- tree view / filter
- drag & drop payload の標準化

へ進められます。

## 20. 現時点の限界と読み方

この実装は完成品というより、次の価値を持つ段階のコードです。

- Qt shell と runtime の接続を確認する
- Docking 体験を検証する
- Scene/Game 2 viewport 構成を試す
- 既存 ImGui editor を段階的に置き換える足場を作る

したがって、読むときは

- どこが本番相当の責務か
- どこが UI モックか
- どこが将来の拡張点か

を分けて理解すると誤解しにくいです。

## 21. 学習用チェックポイント

コードを読んだあとに確認すると理解が深まる観点を列挙します。

- なぜ `QtEditorFrontend` 自体は薄く、`MainWindow` に集中しているのか
- なぜ `RuntimeBridge` は C export しか見ていないのか
- なぜ `gameRuntime_` は DLL コピーを使う必要があるのか
- なぜ `SetEditorUiEnabled(false)` が Qt 版成立条件なのか
- なぜ Scene/Game の actor state はまだ厳密同期していないのか
- なぜ `QTimer` で `tick()` しているのか
- なぜ `QDockWidget` ではなく ADS を採用しているのか

## 22. 要約

`EditorQt` の設計を一文で言うと、

「既存の `ApplicationDLL` runtime を壊さずに、Qt と Advanced Docking System で Editor の外側 UI を構築し、Scene / Game のネイティブ描画 window を Qt に埋め込む構成」

です。

その中で中心になる理解ポイントは次の 4 つです。

- UI backend は `IEditorFrontend` で差し替え可能
- Qt 版の本体は `MainWindow`
- Runtime 境界は `RuntimeBridge`
- Scene / Game は 2 つの DLL インスタンスで動いている

この 4 点を押さえると、Qt 周りのコードはかなり読みやすくなります。
