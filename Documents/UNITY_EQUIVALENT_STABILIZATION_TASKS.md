# Unreal風 Actor + Component 設計へ近づけるための安定化タスク

## 目的

このプロジェクトでは、C#側を Unity の `GameObject + Component` そのままではなく、Unreal Engine に近い `Actor + Component` 方式へ寄せる。

目標は次の形にすること。

```text
C# gameplay authority:
Scene
  Actor
    Transform
    Component
      SpriteRenderer
      MeshRenderer
      CameraComponent
      PlayerController

Native runtime:
SpriteRendererInstance
MeshRendererInstance
TextureResource
MeshResource
ViewportCamera
```

重要な方針は、**C# の Scene / Actor / Component を gameplay authority にする**こと。native 側には Actor 階層を複製せず、描画やリソースの実体だけを持たせる。

## 現状との差分

前回の Unity風資料から、次の点を変更する。

- `GameObject` を長期的には `Actor` に置き換える
- `Scene.DestroyGameObject` は `Scene.DestroyActor` 相当へ移行する
- Editor の `worldActors_` は命名上は自然だが、実行 `Actor` と同期する設計に変える
- Component は Unity同様に再利用可能な機能部品として残す
- ゲーム固有の高レベル処理は `Actor` に書いてよい
- 描画、物理、入力補助、音、カメラなどの機能は `Component` に分ける
- native `RuntimeActor` は gameplay Actor ではなく、当面は viewport camera などの内部状態に限定する

## 到達目標

安定化後に目指す状態は次の通り。

- Actor の破棄で、対応する native SpriteRenderer が必ず破棄される
- Component 単体削除でも native resource が残らない
- Editor の Hierarchy/Details は Actor DTO を編集し、PIE開始時に C# Scene へ反映できる
- `GameEntry.CreateScene()` のハードコードから、Editor/Serialized Scene 生成へ移行する道筋がある
- MeshRenderer 実装前に Actor / Component / RendererSystem の責務が固定される
- 3D Transform 実装前に、現在の2D Sprite Transformとの接続方針が明確になる

## Phase 0. 方針固定

### Task 0-1. 用語を Actor + Component 方針に固定する

- 目的: 今後の資料と実装で `GameObject` と `Actor` が混在し続けないようにする
- 対象:
  - `UNITY_STYLE_RUNTIME_REFACTOR.md`
  - `Documents/UNITY_EQUIVALENT_STABILIZATION_TASKS.md`
- 作業:
  - 長期用語は `Actor` にする
  - 現在の `GameObject` は移行前の名前だと明記する
  - `Component` は維持する
- 完了条件:
  - 新規設計で `GameObject` を増やすべきでないことが分かる

### Task 0-2. Actor と Component の責務を明文化する

- 目的: Unreal風にしても巨大 Actor へ崩れないようにする
- 作業:
  - Actor は identity、lifecycle、spawn/destroy、所有 Component、ゲーム固有の高レベル処理を持つ
  - Component は描画、物理、音、入力補助、カメラなどの再利用機能を持つ
  - native handle は原則 Component または System が管理する
- 完了条件:
  - `PlayerActor` に何を書き、`SpriteRenderer` に何を書くか判断できる

### Task 0-3. 現在の GameObject を Actor 移行前クラスとして扱う

- 目的: すぐ大規模リネームせずに設計を前進させる
- 対象: `PieGameManaged/GameObject.cs`
- 作業:
  - まず資料上で `GameObject` は `Actor` の暫定名と定義する
  - 後続タスクで `Actor.cs` へリネームするか、新規 `Actor` を作って移行するか決める
- 完了条件:
  - 実装修正前でも設計上の主語が Actor に揃う

## Phase 1. Actor破棄と native resource 寿命を安定させる

### Task 1-1. Actor破棄時の SpriteRenderer 残留を再現する

- 目的: 現在の `DestroyGameObject` 問題を Actor設計の最初の不具合として固定する
- 対象: `PieGameManaged/GameEntry.cs`
- 作業:
  - 一定時間後に Player を破棄する仮コードを作る
  - 描画が残るか、native handle が残るか確認する
- 完了条件:
  - 修正前後を比較できる

### Task 1-2. Component破棄通知の方式を決める

- 目的: Actor が Scene から消えても、Component の native resource を破棄できるようにする
- 候補:
  - `Component.OnDestroy()` から直接 native を解放する
  - `Scene` が destroyed component queue を持つ
  - `RendererSystem` が明示的に `Release(component)` を受ける
- 推奨:
  - native handle の解放は RendererSystem 側に寄せる
  - `Scene` または `Actor` から destroyed component queue へ通知する
- 完了条件:
  - 破棄通知の実装方式が1つに決まっている

### Task 1-3. SpriteRendererSystem に Component単体Releaseを追加する

- 目的: Scene全体ではなく、対象 Component だけを解放できるようにする
- 対象: `PieGameManaged/SpriteRendererSystem.cs`
- 作業:
  - `Release(SpriteRenderer spriteRenderer)` を追加する
  - 既存の `DestroyNativeSpriteRenderer` を再利用する
- 完了条件:
  - Actor破棄、Component削除、PIE停止のどこからでも単体解放できる

### Task 1-4. Scene/Actor から destroyed component を取得できるようにする

- 目的: Sync対象から消えた Component を RendererSystem が処理できるようにする
- 対象:
  - `PieGameManaged/Scene.cs`
  - `PieGameManaged/GameObject.cs`
- 作業:
  - `ComponentDestroyed` event または queue を追加する
  - Actor破棄時と Component削除時に通知する
- 完了条件:
  - 削除済み `SpriteRenderer` を `SpriteRendererSystem.Sync` が取得できる

### Task 1-5. SpriteRendererSystem.Sync で destroyed queue を処理する

- 目的: Actor一覧の走査前に削除済み Component の native resource を破棄する
- 対象: `PieGameManaged/SpriteRendererSystem.cs`
- 作業:
  - Sync冒頭で destroyed components を処理する
  - `SpriteRenderer` だけを解放対象にする
- 完了条件:
  - Actor破棄後に `NativeSpriteRendererHandle` が 0 になる

### Task 1-6. PIE停止時の二重解放を確認する

- 目的: queue方式追加後に `Release(scene)` と destroyed queue が衝突しないようにする
- 対象: `PieGameManaged/GameEntry.cs`
- 正しいPIE停止順序（この順でなければ二重解放または handle 残留が起きる）:
  1. `SpriteRendererSystem.ProcessDestroyedQueue()` — キューに残った handle を先に解放する
  2. `SpriteRendererSystem.Release(scene)` — 残存 handle を一括解放する
  3. `Scene.DestroyAllGameObjects()` — C# オブジェクトを解放する
- 注意: `Release()` 内部では `NativeSpriteRendererHandle == 0` の Component をスキップする防衛コードを入れる
- 完了条件:
  - PIE停止時に native resource が一度だけ解放される

## Phase 2. Actor API へ移行する

### Task 2-1. `Actor` クラスの最小形を決める

- 目的: GameObject名のまま拡張し続けない
- 最小構造:
  - `Name`
  - `ActiveSelf`
  - `Transform`
  - `AddComponent<T>()`
  - `GetComponent<T>()`
  - `BeginPlay`
  - `Tick`
  - `EndPlay`
- 完了条件:
  - `Actor.cs` を作るか、`GameObject.cs` をリネームするか決められる

### Task 2-2. Actor lifecycle と Component lifecycle の順序を決める

- 目的: Unreal風にした時の実行順序を固定する
- 命名方針（要決定）: Component 側が既に Unity の `Awake / Start / Update / OnDestroy` を持つため、
  Actor 側も同名に揃える（Unity寄り統一）。`BeginPlay` は `Start` の alias として残してよい。
  Unreal寄りに全部 `BeginPlay/Tick/EndPlay` へ統一する場合は Component.cs の変更が必要になる。
  **混在（Component=Unity名 / Actor=Unreal名）は避ける。**
- 推奨順（Unity寄り統一を採用した場合）:
  - Actor constructed
  - Component added
  - Component Awake
  - Actor Awake / BeginPlay
  - Component Start
  - Actor Start / BeginPlay
  - Actor Tick / Update
  - Component Update
  - Component OnDestroy
  - Actor OnDestroy / EndPlay
- 完了条件:
  - 命名方針が1つに決まり、ActorとComponentのどちらに初期化を書くか判断できる

### Task 2-3. Actor固有処理の許容範囲を決める

- 目的: Actor巨大化を防ぐ
- Actorに書いてよいもの:
  - PlayerActor の入力方針
  - EnemyActor の状態遷移
  - Spawn時のComponent構成
  - Actor単位のTick
- Componentに分けるもの:
  - SpriteRenderer
  - MeshRenderer
  - CameraComponent
  - AudioSource
  - Collider
  - Healthなど再利用したい機能
- 完了条件:
  - 「全部Componentに書く」でも「全部Actorに書く」でもない基準がある

### Task 2-4. `Scene.CreateActor<T>()` 方針を決める

- 目的: Unreal風の spawn に近い入口を作る
- 候補:
  - `Scene.CreateActor(string name)`
  - `Scene.SpawnActor<TActor>() where TActor : Actor, new()`
  - `Scene.DestroyActor(Actor actor)`
- 完了条件:
  - Editor/Serialized Scene から Actor を生成する入口が決まる

### Task 2-5. 既存 GameObject API の互換方針を決める

- 目的: 一気に壊さず移行する
- 候補:
  - `GameObject` を `Actor` にリネームする
  - `Actor` を新設し、`GameObject` を一時ラッパーにする
  - 互換なしで全置換する
- 推奨:
  - 小規模ならリネーム
  - 影響が大きければ `Actor` 新設後に段階移行
- 完了条件:
  - 次の実装PRの差分範囲が見える

## Phase 3. Editor worldActors_ を Actor DTO として整理する

### Task 3-1. `EditorWorldActor` を EditorActor DTO と定義する

- 目的: Editor側データを実行 Actor と同期するための中間表現にする
- 対象: `EditorQt/src/MainWindow.h`
- 作業:
  - `EditorWorldActor` は runtime Actor ではなく Editor DTO と明記する
  - `actorName` は表示名、IDは別に持つ方針にする
- 完了条件:
  - Editor UI専用リストではなく、Scene生成元として扱える

### Task 3-2. EditorActorId を導入する

- 目的: 名前ベース同期を避ける
- 対象: `EditorWorldActor`
- 方針: **GUID (128-bit UUID)** を採用する。`uint32_t` インクリメント方式はセッションをまたぐとIDが変わり、
  Scene ファイルを保存→読み込みした時に同一Actorを追跡できなくなる。Undo/Redo も壊れる。
  - C++ 側: `std::string guid` (ハイフンなし32文字、`CoCreateGuid()` または `UuidCreate()` で生成)
  - C# 側: `System.Guid` 型
- 作業:
  - `EditorWorldActor` に `std::string editorActorGuid` を追加する
  - Actor生成時に GUID を発行する
  - 同名 Actor を許容できるようにする
- 完了条件:
  - Hierarchy、Details、PIE生成で同一Actorを追跡できる

### Task 3-3. ActorのComponent構成をEditor DTOに持たせる方針を決める

- 目的: Actorだけでなく Component も編集対象にする
- 最小構成:
  - Transform
  - SpriteRenderer settings
  - MeshRenderer settings
  - Camera settings
- 完了条件:
  - Content Browser からDropした asset が、どの Component として追加されるか決まる

### Task 3-4. Drag&Drop Spawn を Actor生成として扱う

- 目的: Content Browser の操作を Scene変更として扱う
- 対象: `spawnActorFromAssetPath`
- 作業:
  - Mesh asset は MeshRenderer付き Actor を作る
  - Texture asset は SpriteRenderer付き Actor を作る
  - Light asset は LightComponent付き Actor を作る
  という方針を資料化する
- 完了条件:
  - Drag&Drop が単なる `worldActors_` 追加で終わらない

### Task 3-5. Details編集と runtime反映の境界を分ける

- 目的: UIイベントから直接 runtime API が散らばらないようにする
- 対象: `EditorQt/src/MainWindow.cpp`
- 作業:
  - spinbox変更は EditorActor DTO を更新する
  - runtime反映は `ApplyEditorActorToRuntime(actor)` に集約する
- 完了条件:
  - PIE中/停止中で同じ Editor DTO を使える

### Task 3-6. PIE開始時に Editor DTO から C# Scene を生成する方針を決める

- 目的: Unreal/Unityと同じく「編集したSceneを再生する」流れに近づける
- 候補:
  - Editor DTOをJSONにして C# に渡す
  - native経由で Actor spawn API を呼ぶ
  - C#側が scene asset を読む
- 推奨:
  - 短期は JSON scene asset
  - 中期で Scene serialization に統合
- 完了条件:
  - `GameEntry.CreateScene()` のハードコードから抜ける道筋がある

## Phase 4. native RuntimeActor の扱いを整理する

### Task 4-1. native RuntimeActor を gameplay Actor として使わない方針を固定する

- 目的: C# Actor と native RuntimeActor の二重管理を避ける
- 対象:
  - `ApplicationDLL/AppRuntime.h`
  - `ApplicationDLL/AppRuntime.cpp`
- 作業:
  - native `RuntimeActor` は当面 viewport camera 内部状態に限定する
  - gameplay Actor は C# に置くと明記する
- 完了条件:
  - native側に Actor/Componentツリーを増やさない判断基準がある

### Task 4-2. MainCamera の短期所有者を決める

- 目的: Editor / native / C# の三重管理を避ける
- 推奨:
  - 短期は native viewport camera として扱う
  - C# `CameraComponent` 導入時に Actor化を検討する
- `RuntimeActor` 構造体のリネーム方針:
  - 現在の `RuntimeActor` は viewport camera 内部状態のみを持つため、名称が誤解を招く
  - `RuntimeActor` → `ViewportCameraState` にリネームする
  - `g_runtimeActors` → Game/Scene 2つを `g_gameViewportCamera` / `g_sceneViewportCamera` として明示的に持たせる（複数管理不要）
  - `ApplicationDLL/AppRuntime.h` で変更する
- 完了条件:
  - `MainCamera` が gameplay Actor なのか viewport設定なのか決まっている
  - native 側に gameplay Actor 構造体が存在しないことが名前で分かる

### Task 4-3. native API は Component単位に限定する

- 目的: nativeが gameplay Actor を所有しないようにする
- 良いAPI:
  - `CreateSpriteRenderer`
  - `DestroySpriteRenderer`
  - `SetSpriteRendererTransform`
  - `CreateMeshRenderer`
  - `SetMeshRendererMesh`
- 避けるAPI:
  - `CreateActor`
  - `AddComponentToActor`
  - `SetActorParent`
  ただし、native側に完全なScene同期を作ると決めた場合は別途設計する
- 完了条件:
  - native APIの責務が rendering/resource に限定される

## Phase 5. GameEntry のサンプル依存を薄くする

### Task 5-1. SampleSceneFactory を分離する

- 目的: `GameEntry` をランタイム接続に戻す
- 対象: `PieGameManaged/`
- 作業:
  - `SampleSceneFactory` を作る方針を決める
  - PlayerActor生成をそこへ移す
- 完了条件:
  - `GameEntry` は Start/Tick/Stop の制御に集中する

### Task 5-2. PlayerActor サンプルを作る方針を決める

- 目的: Unreal風の書き方の基準を作る
- 例:
  - `PlayerActor : Actor`
  - BeginPlayで `SpriteRenderer` と `PlayerPulseComponent` を追加
  - Tickで高レベル処理を書く
- 完了条件:
  - Actorに処理を書くサンプルと Componentに処理を書くサンプルの境界が見える

### Task 5-3. Actor Tick と Component Update の順序を実装に反映する

- 目的: lifecycle を設計だけで終わらせない
- 対象:
  - `Scene.cs`
  - `Actor.cs` または `GameObject.cs`
  - `Component.cs`
- 作業:
  - Actor Tick
  - Component Update
  の呼び順を固定する
- 完了条件:
  - 毎フレーム処理の責務が安定する

## Phase 6. Transform の移行境界を固める

### Task 6-1. 現在の Transform を暫定2D Transformとして扱う

- 目的: MeshRenderer用3D Transformと混同しない
- 対象: `PieGameManaged/Transform.cs`
- 作業:
  - 現在の `CenterX/CenterY/Width/Height` は Sprite用暫定仕様と明記する
  - すぐのリネームは避けるか、影響範囲を見て判断する
- 完了条件:
  - MeshRenderer追加時にTransform仕様で迷わない

### Task 6-2. Actor Transform の3D最小構造を決める

- 目的: Unreal風 Actor の基本変換を決める
- 最小候補:
  - `Vector3 Location`
  - `Vector3 Rotation`
  - `Vector3 Scale`
  - `Matrix4x4 LocalToWorld`
- 完了条件:
  - `SetMeshRendererTransform()` の引数方針が決まる

### Task 6-3. SpriteRenderer と MeshRenderer の Transform共有方針を決める

- 目的: 2D Sprite と 3D Mesh を同じ Actor設計で扱えるようにする
- 推奨:
  - 長期は Actor `Transform` を3D化する
  - SpriteRenderer はXYとScaleを使う
  - UI用途が必要になったら `RectTransform` を別導入する
- 完了条件:
  - SpriteRenderer既存APIをどう移行するか見える

### Task 6-4. SpriteRendererSystem の座標変換を 3D Transform 対応にする

- 目的: Actor Transform が 3D 化した後も SpriteRenderer が正しく描画されるようにする
- 背景: 現在の `SpriteRendererSystem` は 2D viewport camera の NDC 変換を行っており、
  `CenterX/CenterY/Width/Height` を直接 native に渡している。
  3D Transform に移行すると `Vector3 Location` の XY を viewport 座標に変換する処理が必要になる。
- 作業:
  - `Actor.Transform.Location.XY` を viewport 座標として `SetSpriteRendererTransform()` に渡す変換を決める
  - `Transform.Width/Height` を `Transform.Scale.XY` から取得するよう変更する方針を決める
  - `SpriteRendererSystem.Sync()` を 3D Transform 前提に更新する
- 完了条件:
  - SpriteRenderer が 3D Actor Transform を受け取っても正しく表示される

## Phase 7. MeshRenderer 着手前チェック

### Task 7-1. MeshRenderer 前提条件を確認する

- 条件:
  - Actor破棄で native SpriteRenderer が漏れない
  - Component単体削除で native resource が漏れない
  - PIE停止時の resource 二重解放が起きない（Task 1-6 の正しい順序が実装済み）
  - EditorActor DTO と C# Actor の同期方針がある（GUID方式、Task 3-2 完了）
  - Transform の2D/3D方針がある（Task 6-1〜6-4 完了）
  - SpriteRendererSystem が 3D Transform に対応している（Task 6-4 完了）
  - nativeは Actor階層を持たず、render instanceだけを持つ方針が守られている
  - `RuntimeActor` が `ViewportCameraState` 相当に整理されている（Task 4-2 完了）
- 完了条件:
  - MeshRenderer実装へ進む判断ができる

### Task 7-2. MeshAsset と PMDAnalyzer の責務を固定する

- 目的: PMD専用描画に戻らないようにする
- 対象:
  - `ApplicationDLL/Analyzer/PMDAnalyzer.*`
  - `ApplicationDLL/Renderer/MeshObject.*`
- 作業:
  - PMDAnalyzerはCPU Meshデータを作るだけにする
  - DX12 resource作成は MeshRenderObject に寄せる
- 完了条件:
  - `Importer -> MeshAsset -> MeshRenderer Component -> RenderObject` の流れが崩れない

### Task 7-3. MeshRenderer API の最小セットを確定する

- 最小API:
  - `LoadMesh(path)`
  - `ReleaseMesh(handle)`
  - `CreateMeshRenderer()`
  - `DestroyMeshRenderer(handle)`
  - `SetMeshRendererMesh(renderer, mesh)`
  - `SetMeshRendererTransform(renderer, transform)`
- 後回し:
  - material array
  - submesh editing
  - animation
  - skinning
- 完了条件:
  - 最初のMeshRendererは単色PMD表示までに制限されている

## 推奨実施順

1. Task 0-1
2. Task 0-2
3. Task 0-3
4. Task 1-1
5. Task 1-2
6. Task 1-3
7. Task 1-4
8. Task 1-5
9. Task 1-6
10. Task 2-1
11. Task 2-2
12. Task 2-3
13. Task 2-4
14. Task 2-5
15. Task 3-1
16. Task 3-2
17. Task 3-3
18. Task 3-4
19. Task 3-5
20. Task 3-6
21. Task 4-1
22. Task 4-2
23. Task 4-3
24. Task 5-1
25. Task 5-2
26. Task 5-3
27. Task 6-1
28. Task 6-2
29. Task 6-3
30. Task 6-4
31. Task 7-1
32. Task 7-2
33. Task 7-3

## 作業ルール

- 1タスクは15分程度で終わる粒度にする
- Actor移行、Editor同期、MeshRenderer実装を同じ変更に混ぜない
- native側に gameplay Actor階層を増やさない
- Actorに処理を書いてよいが、再利用したい機能はComponentへ逃がす
- PMD固有処理を MeshRenderer の名前に混ぜない
- ビルド可能な単位で止める

## 安定化完了の判定

次の条件を満たしたら、MeshRenderer / 3D Transform の実装に進んでよい。

- `DestroyActor` 相当の処理で native SpriteRenderer が残らない
- `RemoveComponent<SpriteRenderer>` で native SpriteRenderer が残らない
- PIE停止時に native resource の二重解放が起きない
- EditorActor DTO から C# Scene / Actor を生成する方針がある
- C# Actor が gameplay authority、native は render/resource authority という分担が守られている
- Actor Transform の3D移行方針が決まっている
- MeshRenderer の最小APIが決まっている
