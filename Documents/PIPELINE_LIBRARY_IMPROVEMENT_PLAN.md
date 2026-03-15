# PipelineLibrary 改善手順書

## 1. 目的

このドキュメントは、`ApplicationDLL/Renderer/PipelineLibrary.*` を安全に改善していくための実施手順をまとめたものです。

対象は次の課題です。

- `PipelineDesc` に責務が集まりすぎている
- シェーダーコンパイルと PSO キャッシュが密結合
- 失敗時の診断情報が弱い
- Debug/Release の振る舞いが分かれていない
- 将来の拡張に向けた構造がまだ弱い

この手順書では、破壊的変更を一気に入れず、段階的に改善する流れを採用します。

## 2. 前提理解

現状の `PipelineLibrary` は、次をまとめて担当しています。

- `PipelineDesc` をキーにしたキャッシュ
- HLSL コンパイル
- Root Signature 作成
- Graphics PSO 作成
- `shared_ptr<const Pipeline>` の返却

つまり現在は「描画パイプラインの生成器」と「メモリ上キャッシュ」が一体化した設計です。

## 3. 改善の基本方針

改善は以下の順番で進めます。

1. 観測しやすくする
2. 既存責務を分離する
3. キャッシュ粒度を適切化する
4. 拡張に備えた形へ整理する

この順番にする理由は、先に分解だけ進めると、どこで壊れたか分からなくなるためです。  
まず可視化を入れてから構造変更に進む方が安全です。

## 4. 改善全体ロードマップ

### Phase 1. 可視化と安全確認

- キャッシュヒット/ミスの可視化
- 失敗時ログの強化
- Debug/Release のコンパイルフラグ整理

### Phase 2. 責務分離

- `PipelineDesc` の分割
- シェーダーキャッシュ分離
- Root Signature キャッシュ分離

### Phase 3. 拡張性向上

- Graphics / Compute の型分離
- 競合抑止の明確化
- 永続キャッシュ対応の下地作り

## 5. 詳細手順

## Step 1. 現状把握用のログを追加する

### 目的

改善前後で次を比較できるようにします。

- 何回 `GetOrCreate()` が呼ばれたか
- キャッシュヒットした回数
- 新規生成した回数
- 失敗した回数

### 変更対象

- `ApplicationDLL/Renderer/PipelineLibrary.h`
- `ApplicationDLL/Renderer/PipelineLibrary.cpp`

### 実施内容

- `PipelineLibrary` に統計カウンタを追加する
- 例:
  - `totalRequests`
  - `cacheHits`
  - `cacheMisses`
  - `createFailures`
- `GetOrCreate()` 内でこれらを更新する
- 必要なら `DumpStats()` のような関数を追加する

### 期待効果

- 改善後にキャッシュ効率が良くなったか判断できる
- 不必要な PSO 再生成の発見がしやすくなる

### 確認項目

- 同じマテリアルを複数回作ってもミス回数が増えすぎないか
- ログがフレームごとに大量出力されないか

## Step 2. 失敗時ログを強化する

### 目的

`CreateGraphicsPipelineState failed` だけでは原因特定が難しいため、失敗時に設定内容を確認できるようにします。

### 変更対象

- `PipelineLibrary.h`
- `PipelineLibrary.cpp`

### 実施内容

- `PipelineDesc` の内容を文字列化する関数を追加する
- 例:
  - `std::string DescribePipelineDesc(const PipelineDesc&)`
- 失敗時に以下をログ出力する
  - VS/PS ファイル名
  - EntryPoint
  - ShaderModel
  - input layout 数
  - root parameter 数
  - sampler 数
  - blend/depth/cull 設定

### 期待効果

- 入力レイアウトやルートシグネチャ不整合を追いやすくなる
- 初心者でも何を作ろうとして失敗したか理解しやすい

### 確認項目

- ログに必要十分な情報が出るか
- 長すぎて読めなくなっていないか

## Step 3. シェーダーコンパイルフラグを構成別に切り替える

### 目的

現在は `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION` 固定なので、Release 動作に向いていません。

### 変更対象

- `PipelineLibrary.cpp`

### 実施内容

- compile flags を定数直書きから関数化する
- 例:
  - `UINT GetShaderCompileFlags()`
- Debug:
  - `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION`
- Release:
  - 最適化有効
  - デバッグ情報なし

### 期待効果

- Release 実行時の不要なコストを減らせる
- Debug と Release の意図が明確になる

### 確認項目

- Debug ビルドで従来通りのデバッグ性が保たれるか
- Release ビルドで正常にシェーダーコンパイルできるか

## Step 4. PipelineDesc を責務ごとに分割する

### 目的

現在の `PipelineDesc` は大きすぎて、比較・ハッシュ・拡張が難しいため分割します。

### 分割案

- `ShaderProgramDesc`
  - VS/PS ファイル、EntryPoint、ShaderModel
- `RootSignatureDesc`
  - RootParameters
  - StaticSamplers
- `GraphicsPipelineStateDesc`
  - RT format
  - cull
  - topology
  - depth
  - blend
  - input layout

### 変更対象

- `PipelineLibrary.h`
- `PipelineLibrary.cpp`
- `Material.h`
- `Material.cpp`

### 実施内容

- 既存 `PipelineDesc` の中身を新構造体へ移す
- `PipelineDesc` は最終的に
  - `shader`
  - `rootSignature`
  - `graphics`
  の合成にする
- `operator==` と hash を各構造体ごとに分離する

### 期待効果

- どの設定が変わるとキャッシュキーが変わるのか理解しやすい
- RootSignature や Shader の再利用戦略を分けやすい

### 確認項目

- 既存 `Material::CreateBuiltInTexturedQuadDesc()` の挙動が変わらないか
- ハッシュと等値比較の整合性が崩れていないか

## Step 5. ShaderCache を分離する

### 目的

シェーダーコンパイルは PSO キャッシュとは別の責務なので、独立したキャッシュに分離します。

### 新規追加候補

- `ApplicationDLL/Renderer/ShaderCache.h`
- `ApplicationDLL/Renderer/ShaderCache.cpp`

### 実施内容

- キー:
  - shader file
  - entry point
  - shader model
  - compile flags
- 値:
  - `ComPtr<ID3DBlob>`
- `PipelineLibrary` は `ShaderCompiler::CompileFromFile()` を直接呼ばず、`ShaderCache::GetOrCreate()` を使う

### 期待効果

- 同じシェーダーを複数 PSO で使う場合の無駄を減らせる
- `PipelineLibrary` の責務を小さくできる

### 確認項目

- 同じ VS/PS の再コンパイルが減るか
- ファイル差し替え時に更新検知方針をどうするか

## Step 6. RootSignatureCache を分離する

### 目的

Root Signature も PSO とは別軸で共有できるため、再利用単位を分けます。

### 新規追加候補

- `ApplicationDLL/Renderer/RootSignatureCache.h`
- `ApplicationDLL/Renderer/RootSignatureCache.cpp`

### 実施内容

- `RootSignatureDesc` をキーに Root Signature をキャッシュする
- `PipelineLibrary` は
  - ShaderBlob
  - RootSignature
  - GraphicsPSO
  を組み合わせるだけに近づける

### 期待効果

- 同一ルートシグネチャを使う複数PSOで無駄が減る
- DX12 の概念ごとの責務整理が進む

### 確認項目

- 既存のテクスチャ付き quad 描画で問題が出ないか
- Root Parameter 順序が変わらず維持されるか

## Step 7. PipelineLibrary を GraphicsPSO キャッシュへ明確化する

### 目的

責務分離後の `PipelineLibrary` を、名前通りの「PSO キャッシュ」に近づけます。

### 実施内容

- `CreatePipeline()` の中でやることを減らす
- 最終的に以下だけを担当させる
  - `GraphicsPipelineStateDesc` をキーにキャッシュ
  - `CreateGraphicsPipelineState()` 呼び出し
- クラス名を将来的に `GraphicsPipelineCache` へ変えることも検討する

### 期待効果

- クラス名と役割のズレが減る
- 今後 compute pipeline を入れるときに整理しやすい

### 確認項目

- 呼び出し側のインターフェース変更が過大になっていないか

## Step 8. in-flight 生成競合を抑止する

### 目的

将来的に並列読み込みを行う場合、同じキーの PSO を複数スレッドが同時に作る無駄を防ぎます。

### 実施内容

- 現在は
  - lock で検索
  - lock を外して生成
  - 再度 lock で挿入
  となっている
- この方式だと並列時に同じ PSO を二重生成する可能性がある
- 改善案:
  - in-flight マップを持つ
  - `future` 相当で待ち合わせる
  - あるいは生成中フラグ付きエントリを持つ

### 期待効果

- アセットロードの並列化に備えられる

### 確認項目

- 現時点で本当に必要か
- 先に複雑化しすぎないか

## Step 9. Graphics / Compute を分離できる設計にしておく

### 目的

今後 compute shader を扱うなら、graphics 専用構造のままでは拡張が難しいです。

### 実施内容

- `PipelineKind` 追加を検討
- ただし今すぐ compute 実装を入れる必要はない
- まずは API だけ拡張余地を作る

例:

- `enum class PipelineKind { Graphics, Compute }`

### 期待効果

- 将来的な compute 対応時に大きな作り直しを避けやすい

### 確認項目

- 現在のコードが過剰抽象化にならないか

## Step 10. 永続キャッシュは最後に検討する

### 目的

ディスク上への PSO 永続化は魅力がありますが、最初に手を出すべき領域ではありません。

### 理由

- まずメモリ上キャッシュと責務分離を安定させるべき
- 永続化はバージョン管理や無効化戦略が必要
- シェーダー変更時の整合性管理が難しい

### 結論

永続キャッシュは Phase 3 以降の検討事項とします。

## 6. 実施順の推奨

最小リスクで進めるなら、次の順番が推奨です。

1. Step 1: 統計ログ追加
2. Step 2: 失敗時ログ強化
3. Step 3: compile flags 切替
4. Step 4: `PipelineDesc` 分割
5. Step 5: `ShaderCache` 分離
6. Step 6: `RootSignatureCache` 分離
7. Step 7: `PipelineLibrary` の責務明確化
8. Step 8: 並列生成競合対策
9. Step 9: compute 拡張余地
10. Step 10: 永続キャッシュ検討

## 7. 変更ごとのテスト観点

各ステップで最低限確認すべき内容をまとめます。

### 描画確認

- quad が従来通り表示されるか
- テクスチャ付き quad が正しく表示されるか
- 描画が真っ黒にならないか

### キャッシュ確認

- 同一マテリアル生成時にヒットするか
- 不要な再コンパイルが起きていないか
- `Clear()` 後に再生成できるか

### 失敗系確認

- 存在しない shader を指定したときに分かりやすいログが出るか
- root parameter の不整合時に原因が追えるか

### 回帰確認

- `Material::Initialize()` の呼び出し側が大きく壊れていないか
- `QuadRenderObject` 初期化が失敗しないか

## 8. 実装時の注意

### 8.1 一気に全部やらない

特に以下は同時にやらない方が安全です。

- `PipelineDesc` 分割
- `ShaderCache` 分離
- `RootSignatureCache` 分離

これらを同時に進めると、どこで壊れたか切り分けにくくなります。

### 8.2 ハッシュと等値比較は必ずセットで直す

キャッシュキーの構造を変更したら、以下を必ず同時に更新します。

- `operator==`
- hasher

どちらかだけ変えるとキャッシュ不整合の原因になります。

### 8.3 ログは出しすぎない

毎フレーム出るログは避けるべきです。  
統計ログは必要に応じて明示的に出す形が望ましいです。

## 9. 完成形のイメージ

最終的には次の分担が理想です。

- `ShaderCache`
  - シェーダーバイトコードのキャッシュ
- `RootSignatureCache`
  - Root Signature のキャッシュ
- `GraphicsPipelineCache`
  - Graphics PSO のキャッシュ
- `Material`
  - 描画時に必要なパラメータを束ねる
- `QuadRenderObject`
  - メッシュと描画呼び出し

この状態になると、DX12 の主要概念ごとに責務が分かれ、初心者でも追いやすい構成になります。

## 10. まとめ

`PipelineLibrary` の改善は、単にコードをきれいにするだけではなく、次の価値があります。

- 描画失敗の調査がしやすくなる
- 同じ設定の再利用効率が上がる
- DX12 の概念ごとの責務分離が進む
- 将来のマテリアル増加や compute 対応に備えやすくなる

重要なのは、最初に大きく作り変えず、

- 観測
- 分離
- 再利用単位の整理
- 拡張性確保

の順で進めることです。
