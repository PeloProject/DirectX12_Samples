# Unity風 Mesh / MeshRenderer 実装手順

## 目的

PMDモデルを直接描画クラスへつなぐのではなく、Unityに近い形で次の構造へ整理する。

```text
Importer(PMDAnalyzer)
  -> Mesh
  -> MeshRenderer
  -> RenderObject
  -> Backend(DX12)
```

PMDは「Meshを作る入力形式」の1つとして扱う。実行時の描画はPMD専用ではなく、`Mesh` と `MeshRenderer` を中心にする。

## 最終形のイメージ

### C#側

```csharp
var miku = scene.CreateGameObject("Miku");
miku.Transform.Location = new Vector3(0, 0, 0);
miku.Transform.Scale = new Vector3(1, 1, 1);

var renderer = miku.AddComponent<MeshRenderer>();
renderer.Mesh = Mesh.Load("Assets/Model/初音ミク.pmd");
renderer.Materials = new[]
{
    BuiltInMaterials.PmdUnlit
};
```

### ネイティブ側

```text
MeshAsset
  vertices
  indices
  subMeshes
  sourcePath

MeshRenderObject
  MeshAsset
  Material[]
  Transform
  vertex/index buffers

AppRuntime
  LoadMesh()
  CreateMeshRenderer()
  SetMeshRendererMesh()
  SetMeshRendererMaterial()
  SetMeshRendererTransform()
```

## 責務分割

### PMDAnalyzer / PMDImporter

PMDファイルを読み、CPU側の `MeshAsset` データを作る。

やること:

- PMDヘッダーを読む
- 頂点配列を読む
- インデックス配列を読む
- 材質情報を読む
- subMesh情報へ変換する

やらないこと:

- DX12リソースを作らない
- command listへ描画命令を積まない
- shaderやPSOを直接持たない

### MeshAsset

Unityの `Mesh` に近いCPU側アセット。

```cpp
struct MeshVertex
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    uint16_t boneIndex[2];
    uint8_t boneWeight;
    uint8_t edgeFlag;
};

struct SubMesh
{
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

struct MeshAsset
{
    std::string sourcePath;
    std::vector<MeshVertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<SubMesh> subMeshes;
};
```

### MeshRenderObject

Unityの `MeshRenderer` が最終的に描画へ渡す実体に近いクラス。

やること:

- `MeshAsset` から vertex/index buffer を作る
- `Material` を初期化する
- transform用 constant buffer を更新する
- `Render()` で `DrawIndexedInstanced()` を呼ぶ

### MeshRenderer Component

C#側のUnity風コンポーネント。

```csharp
internal sealed class MeshRenderer : Component
{
    public Mesh? Mesh { get; set; }

    public string[] Materials { get; set; } =
    {
        BuiltInMaterials.PmdUnlit
    };

    internal uint NativeMeshRendererHandle { get; set; }
    internal uint AppliedMeshHandle { get; set; }
}
```

## 実装方針

最初からPMDの全機能を再現しない。

最初のゴール:

```text
PMDを読み込む
-> 頂点とインデックスで三角形を描く
-> 単色で表示される
```

その後で、次の順番で拡張する。

1. PMD材質ごとのsubMesh描画
2. テクスチャ
3. アルファ/両面/カリング
4. トゥーン/スフィアマップ
5. ボーン/スキニング

## 15分タスクリスト

各タスクは「手を付けやすい最小単位」を優先する。ビルドが通る状態をこまめに作る。

### Phase 1. 設計の受け皿を作る

- [ ] `ApplicationDLL/Renderer/MeshAsset.h` を追加し、`MeshVertex`, `SubMesh`, `MeshAsset` を定義する
- [ ] `MeshAsset.h` に必要な include を最小限で追加する
- [ ] `PMDAnalyzer.h` からDX12依存の `ComPtr<ID3D12Resource>` を外す方針をコメントで整理する
- [ ] `PMDAnalyzer` の戻り値候補を決める。例: `bool LoadPMDMesh(const std::string& path, MeshAsset* outMesh)`
- [ ] `PMDAnalyzer` を `PMDImporter` にリネームするか、当面そのまま使うか決めてメモを残す

### Phase 2. PMD読み込みをCPUデータ化する

- [ ] PMD頂点構造体を1バイトアラインにする
- [ ] `sizeof(PMDVertex)` が38バイトになることを `static_assert` で確認する
- [ ] `sizeof(vertices)` を使っている箇所を `vertices.size()` 相当に修正する
- [ ] `fopen_s` 失敗時に `false` を返す形へ変更する
- [ ] PMDシグネチャ `"Pmd"` を確認する処理を追加する
- [ ] PMDヘッダー読み込み失敗時に `false` を返す
- [ ] 頂点数を読み、`MeshAsset::vertices` をリサイズする
- [ ] PMD頂点を `MeshVertex` へ変換する
- [ ] インデックス数を読む
- [ ] `MeshAsset::indices` を読み込む
- [ ] インデックス数が3の倍数かチェックする
- [ ] 最初は全体を1つの `SubMesh` として登録する
- [ ] 読み込み完了後に頂点数・インデックス数をログ出力する

### Phase 3. PMDシェーダーを最低限通す

- [ ] `PMDBasicVertexShader.hlsl` の未定義 `viewport` を修正する
- [ ] `BasicShaderHeader.hlsli` とPMDシェーダーの入出力構造を分離する
- [ ] PMD用VS出力に `normal` を入れるか、まず削除して単色描画に寄せる
- [ ] PMD用PSを黒ではなく白または確認しやすい色にする
- [ ] PMD用 constant buffer 構造を `worldViewProj` 1つに簡略化する
- [ ] `fxc` または既存ビルドでPMDシェーダーがコンパイルできるか確認する

### Phase 4. MaterialにPMD用Descを追加する

- [ ] `Material.h` に `CreateBuiltInPmdUnlitDesc()` を宣言する
- [ ] `Material.cpp` に `CreateBuiltInPmdUnlitDesc()` を追加する
- [ ] vertex shader を `PMDBasicVertexShader.hlsl` にする
- [ ] pixel shader を `PMDBasicPixelShader.hlsl` にする
- [ ] input layout に `POSITION`, `NORMAL`, `TEXCOORD`, `BONE_NO`, `WEIGHT`, `EDGE_FLAG` を入れる
- [ ] 最初は texture binding を使わず constant buffer だけにする
- [ ] root signature は最初 `b0` のCBVのみで作る
- [ ] PMD用MaterialDescが `PipelineLibrary` でPSO作成できるか確認する

### Phase 5. MeshRenderObjectを作る

- [ ] `ApplicationDLL/Renderer/MeshRenderObject.h` を追加する
- [ ] `MeshRenderObject` に `Initialize(std::shared_ptr<MeshAsset>)` を追加する
- [ ] `MeshRenderObject` に `Render(ViewportRenderMode)` を追加する
- [ ] `MeshRenderObject` に vertex buffer メンバを追加する
- [ ] `MeshRenderObject` に index buffer メンバを追加する
- [ ] `MeshRenderObject` に vertex/index buffer view を追加する
- [ ] `MeshRenderObject` に `Material` メンバを追加する
- [ ] `MeshRenderObject` に `DX12FrameConstantBuffer` メンバを追加する
- [ ] `Initialize()` で vertex buffer を作る
- [ ] `Initialize()` で index buffer を作る
- [ ] `Initialize()` でPMD用Materialを初期化する
- [ ] `Render()` で `Material::Bind()` を呼ぶ
- [ ] `Render()` で viewport/scissor を設定する
- [ ] `Render()` で `IASetVertexBuffers()` を呼ぶ
- [ ] `Render()` で `IASetIndexBuffer()` を呼ぶ
- [ ] `Render()` で `DrawIndexedInstanced()` を呼ぶ

### Phase 6. SceneGameで仮表示する

- [ ] `SceneGame.h` に `std::unique_ptr<MeshRenderObject>` を追加する
- [ ] `SceneGame.cpp` で `PMDAnalyzer` から `MeshAsset` を読み込む
- [ ] `SceneGame` コンストラクタで `MeshRenderObject` を初期化する
- [ ] `SceneGame::Render()` から `MeshRenderObject::Render()` を呼ぶ
- [ ] モデルが見えない場合に備え、拡大縮小・位置調整用の仮transformを入れる
- [ ] 単色モデルが表示されるか確認する
- [ ] 表示されたらPMD読み込みログと描画ログを整理する

### Phase 7. ネイティブMesh APIを追加する

- [ ] `RuntimeState` に `g_meshes` を追加する
- [ ] `RuntimeState` に `g_nextMeshHandle` を追加する
- [ ] `AppRuntime::LoadMesh(const char* path)` を追加する
- [ ] `AppRuntime::ReleaseMesh(uint32_t handle)` を追加する
- [ ] `extern "C"` に `LoadMesh` を追加する
- [ ] `extern "C"` に `ReleaseMesh` を追加する
- [ ] `dllmain.cpp` にエクスポート関数を追加する
- [ ] `PieNativeApiTable` に `loadMesh` と `releaseMesh` を追加する
- [ ] `PieLoader.cpp` でAPIテーブルへ関数ポインタを設定する

### Phase 8. ネイティブMeshRenderer APIを追加する

- [ ] `RuntimeState` に `g_meshRenderers` を追加する
- [ ] `RuntimeState` に `g_nextMeshRendererHandle` を追加する
- [ ] `AppRuntime::CreateMeshRenderer()` を追加する
- [ ] `AppRuntime::DestroyMeshRenderer()` を追加する
- [ ] `AppRuntime::SetMeshRendererMesh()` を追加する
- [ ] `AppRuntime::SetMeshRendererMaterial()` を追加する
- [ ] `AppRuntime::SetMeshRendererTransform()` を追加する
- [ ] `extern "C"` にMeshRenderer系関数を追加する
- [ ] `dllmain.cpp` にMeshRenderer系エクスポートを追加する
- [ ] `FrameLoop.cpp` に `RenderMeshRenderers()` を追加する
- [ ] `SceneManager::Render()` 後に `RenderMeshRenderers()` を呼ぶ
- [ ] `DestroyAllSpriteRenderers()` と同等のMeshRenderer破棄関数を検討する

### Phase 9. C#側Meshを追加する

- [ ] `PieGameManaged/Mesh.cs` を追加する
- [ ] `Mesh.Load(string path)` を追加する
- [ ] `Mesh` に `NativeMeshHandle` を持たせる
- [ ] `Mesh` に `Release()` を追加するか、管理方法を決める
- [ ] `NativeMethods.cs` に `LoadMesh` を追加する
- [ ] `NativeMethods.cs` に `ReleaseMesh` を追加する
- [ ] `PieNativeApiTable` のC#定義を更新する
- [ ] `TextureHandle` と同じように `MeshHandle` 型を作るか決める

### Phase 10. C#側MeshRendererを追加する

- [ ] `PieGameManaged/MeshRenderer.cs` を追加する
- [ ] `MeshRenderer.Mesh` プロパティを追加する
- [ ] `MeshRenderer.Materials` プロパティを追加する
- [ ] `NativeMeshRendererHandle` を追加する
- [ ] `AppliedMeshHandle` を追加する
- [ ] `AppliedMaterials` の管理方法を決める
- [ ] `NativeMethods.cs` に `CreateMeshRenderer` を追加する
- [ ] `NativeMethods.cs` に `DestroyMeshRenderer` を追加する
- [ ] `NativeMethods.cs` に `SetMeshRendererMesh` を追加する
- [ ] `NativeMethods.cs` に `SetMeshRendererMaterial` を追加する
- [ ] `NativeMethods.cs` に `SetMeshRendererTransform` を追加する

### Phase 11. MeshRendererSystemを追加する

- [ ] `PieGameManaged/MeshRendererSystem.cs` を追加する
- [ ] `Initialize(Scene scene)` を追加する
- [ ] `Sync(Scene scene)` を追加する
- [ ] `Release(Scene scene)` を追加する
- [ ] 無効なGameObject/ComponentではネイティブRendererを破棄する
- [ ] `Mesh` が変わったときだけ `SetMeshRendererMesh()` を呼ぶ
- [ ] `Material` が変わったときだけ `SetMeshRendererMaterial()` を呼ぶ
- [ ] 毎フレームTransformを `SetMeshRendererTransform()` へ同期する
- [ ] `Scene` に `MeshRendererSystem` を追加する
- [ ] `Scene.Start/Update/Destroy` の流れにMeshRendererSystemを接続する

### Phase 12. サンプルで動作確認する

- [ ] `GameEntry.cs` にPMDモデルを1体出すサンプルを追加する
- [ ] `Mesh.Load("Assets/Model/初音ミク.pmd")` を呼ぶ
- [ ] `AddComponent<MeshRenderer>()` を呼ぶ
- [ ] 黒背景で見えない場合はclear colorかPS色を調整する
- [ ] モデルが画面外ならtransformを調整する
- [ ] モデルが巨大/小さすぎる場合はPMD用スケールを仮適用する
- [ ] Editor Scene Viewで表示されるか確認する
- [ ] Game Viewで表示されるか確認する

### Phase 13. PMD材質とSubMesh対応

- [ ] PMD材質数を読む
- [ ] 各材質のindex countを読む
- [ ] PMD材質ごとに `SubMesh` を作る
- [ ] `MeshAsset` に材質名またはテクスチャ名を保持する構造を追加する
- [ ] `MeshRenderObject::Render()` でsubMeshごとに描画する
- [ ] subMeshごとの `DrawIndexedInstanced()` に変更する
- [ ] 材質スロット数とsubMesh数がずれても落ちないようにする
- [ ] 最初は全subMeshを同じMaterialで描く

### Phase 14. テクスチャ対応

- [ ] PMD材質のテクスチャ名を読む
- [ ] PMDファイルのディレクトリから相対テクスチャパスを解決する
- [ ] `MeshAsset` にテクスチャパスを保存する
- [ ] `MeshRenderObject` で `TextureAssetManager` からテクスチャを取得する
- [ ] PMD用MaterialDescにSRV root parameterを追加する
- [ ] PMD用PixelShaderでテクスチャをsampleする
- [ ] テクスチャなし材質は白テクスチャまたは単色へフォールバックする
- [ ] BMPテクスチャが読めるか確認する
- [ ] `.sph` / `.spa` はいったん無視しても落ちないようにする

### Phase 15. TransformをUnity寄りにする

- [ ] C# `Transform` に3D位置・回転・スケールが足りているか確認する
- [ ] ネイティブ `SetMeshRendererTransform()` を3D transform前提にする
- [ ] `MeshRenderObject` で world matrix を作る
- [ ] view/projectionを一旦固定カメラにする
- [ ] 既存2D viewport cameraとの関係を整理する
- [ ] Game View用3Dカメラを `RuntimeActor MainCamera` から作る方針を決める
- [ ] Scene ViewとGame Viewで同じ3Dカメラを使うか分けるか決める

### Phase 16. 後片付けと安定化

- [ ] PMD読み込み失敗時のエラーログを整理する
- [ ] mesh handleの二重解放を防ぐ
- [ ] renderer破棄時にmesh参照を安全に外す
- [ ] renderer backendがDX12以外のときの挙動を決める
- [ ] `DestroyAllSpriteRenderers()` に相当するMesh破棄処理を追加する
- [ ] `ApplicationDLL.vcxproj` に新規ファイルが含まれているか確認する
- [ ] `.filters` に新規ファイルが含まれているか確認する
- [ ] Debugビルドが通るか確認する
- [ ] Releaseビルドが通るか確認する

## 毎回の作業ルール

1タスクは15分程度に抑える。

1回の作業で守ること:

- まず対象ファイルを読む
- 1つの目的だけ変更する
- ビルド可能な単位で止める
- 詰まったらメモを残して次回再開できるようにする

避けること:

- PMD読み込み、Material、Renderer、C#同期を同時に直す
- 最初からテクスチャやボーンまで入れる
- `PMDAnalyzer` に描画責務を持たせる
- `MeshRenderer` をPMD専用名にする

## 推奨する最初の到達点

まずは以下を目標にする。

```text
SceneGame
  -> PMDAnalyzerでMeshAsset作成
  -> MeshRenderObject初期化
  -> 単色でPMD形状を表示
```

この段階ではC#連携はまだ不要。

理由:

- ネイティブ描画経路の問題とC#同期の問題を分けられる
- PMD読み込みの正しさを早く確認できる
- 後からMeshRendererSystemへ移しても捨てるコードが少ない

