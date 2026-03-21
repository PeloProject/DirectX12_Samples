# PipelineLibrary を GraphicsPSO キャッシュとして明確化する手順書

## 1. このドキュメントの目的

このドキュメントは、改善計画の

- Step 7. `PipelineLibrary` を GraphicsPSO キャッシュへ明確化する

を詳しく説明するためのものです。

ここでいう目的は、単にクラス名を変えることではありません。  
本質は、`PipelineLibrary` の責務を

- shader の取得
- root signature の取得
- graphics PSO の生成とキャッシュ

のうち、**graphics PSO の生成とキャッシュ**
に寄せていくことです。

## 2. まず結論

整理後の `PipelineLibrary` は、次のクラスとして理解できる状態が理想です。

- 入力:
  - コンパイル済み shader
  - 作成済み root signature
  - graphics pipeline state 用の設定
- 出力:
  - `ID3D12PipelineState`

つまり `PipelineLibrary` は、最終的には

**Graphics PSO を作って再利用するキャッシュ**

であるべきです。

## 3. なぜこの整理が必要か

現在の `PipelineLibrary` は、段階的改善の途中で次の責務を抱えやすい状態です。

- Shader をどう取るか
- Root Signature をどう作るか
- Graphics PSO をどう作るか
- それぞれをどうキャッシュするか

これを1クラスでやると、次の問題が起きます。

- エラー時に失敗箇所が切り分けにくい
- キャッシュ単位が混ざる
- 今後 compute pipeline を足すとさらに混乱する
- `PipelineLibrary` という名前と実態がずれる

そのため Step 7 では、

- shader は `ShaderCache`
- root signature は `RootSignatureCache`
- graphics PSO は `PipelineLibrary`

という境界にそろえることが重要です。

## 4. 最終的な責務分担

整理後の責務は次の通りです。

### ShaderCache

責務:

- `ShaderProgramDesc` をキーに shader blob を返す

返すもの:

- `ComPtr<ID3DBlob>`

### RootSignatureCache

責務:

- `RootSignatureDesc` をキーに root signature を返す

返すもの:

- `ComPtr<ID3D12RootSignature>`

### PipelineLibrary

責務:

- shader blob と root signature を使って graphics PSO を作る
- graphics PSO をキャッシュする

返すもの:

- `ComPtr<ID3D12PipelineState>`

## 5. Step 7 の到達目標

このステップの完了条件は次です。

1. `PipelineLibrary` の中に shader compile ロジックがない
2. `PipelineLibrary` の中に root signature 生成ロジックがない
3. `PipelineLibrary` が扱うキャッシュ値の主役が PSO である
4. `CreatePipeline()` の仕事が「Graphics PSO を組み立てること」だけになっている

## 6. 現在の `PipelineLibrary` に残りやすい混線

途中段階では、次のようなコードが残りがちです。

- `ShaderCompiler::CompileFromFile()` を直接呼んでいる
- `D3D12SerializeRootSignature()` を直接呼んでいる
- `CreateRootSignature()` を直接呼んでいる
- `Pipeline` 構造体の中に PSO と RootSignature をセットで持っている

この状態だと、まだ `PipelineLibrary` は「全部入り」のままです。

## 7. 整理の基本方針

整理は次の順で進めます。

1. `PipelineDesc` を PSO 中心に再定義する
2. `CreatePipeline()` の入力を「完成済み部品を組み合わせる形」に寄せる
3. キャッシュ対象を PSO に限定する
4. 必要ならクラス名を将来的に `GraphicsPipelineCache` へ寄せられる形にする

## 8. `PipelineDesc` をどう考えるか

Step 7 では、`PipelineDesc` を

- PSO のキー

として扱いやすい形に整理する必要があります。

### 8.1 `PipelineDesc` が持つべきもの

`PipelineDesc` は、最終的に次を持つのが自然です。

- VS 用 `ShaderProgramDesc`
- PS 用 `ShaderProgramDesc`
- `RootSignatureDesc`
- render target format
- cull mode
- topology type
- depth enable
- blend enable
- input layout

ここで重要なのは、

- shader blob そのものは持たない
- root signature object そのものも持たない

ことです。

なぜなら `PipelineDesc` は「キャッシュキー」であり、
実オブジェクトではなく設定の集合だからです。

### 8.2 `PipelineDesc` が持たない方がよいもの

次は入れない方がよいです。

- `ComPtr<ID3DBlob>`
- `ComPtr<ID3D12RootSignature>`
- `Material` の動的バインド情報
- 実際の GPU リソース

これらは PSO のキャッシュキーではなく、実行時状態です。

## 9. `Pipeline` 構造体をどう整理するか

現在 `Pipeline` が次のような形になっている場合があります。

- `pipelineState`
- `rootSignature`

この構成には2つの考え方があります。

### 選択肢 A. そのまま持つ

```cpp
struct Pipeline
{
    ComPtr<ID3D12PipelineState> pipelineState;
    ComPtr<ID3D12RootSignature> rootSignature;
};
```

利点:

- 呼び出し側が便利
- `Material::Bind()` 側で扱いやすい

弱点:

- `PipelineLibrary` が root signature も所有しているように見える

### 選択肢 B. PSO のみ返す

```cpp
using GraphicsPipelineHandle = ComPtr<ID3D12PipelineState>;
```

利点:

- 責務がきれい

弱点:

- 呼び出し側が root signature も別管理する必要がある

### 推奨

現段階では **A でよい** です。  
ただし意味づけを変えます。

ポイントは、

- root signature を `PipelineLibrary` が生成するのではない
- 外部の `RootSignatureCache` から取得したものを、PSO と一緒に呼び出し側へ返しているだけ

という位置づけにすることです。

つまり「所有責務」と「返却上の便宜」は分けて考えます。

## 10. `CreatePipeline()` をどう変えるか

整理後の `CreatePipeline()` は、次の流れだけを持つべきです。

1. `ShaderCache` から VS blob を取得
2. `ShaderCache` から PS blob を取得
3. `RootSignatureCache` から root signature を取得
4. `D3D12_GRAPHICS_PIPELINE_STATE_DESC` を組み立てる
5. `CreateGraphicsPipelineState()` を呼ぶ
6. `Pipeline` に格納して返す

### 10.1 消すべきもの

この時点で `CreatePipeline()` から削除すべき処理は次です。

- shader compile の直接呼び出し
- root signature serialize
- root signature create
- root parameter 変換ロジック
- static sampler 変換ロジック

### 10.2 残すべきもの

残すべき処理は次です。

- shader blob の受け取り
- root signature の受け取り
- rasterizer / blend / depth / input layout 設定
- `CreateGraphicsPipelineState()`

## 11. 実施手順

## Step 7-1. `PipelineDesc` を PSO 視点で見直す

### 目的

「PSO を一意に決める要素」だけを `PipelineDesc` に残すことです。

### やること

- shader 設定を `ShaderProgramDesc` に統一
- root signature 設定を `RootSignatureDesc` に統一
- blend / depth / topology / input layout を整理

### 確認項目

- 同じ `PipelineDesc` なら同じ PSO になるか
- 違う `PipelineDesc` なら別 PSO になるか

## Step 7-2. `CreatePipeline()` から shader compile を外す

### 目的

`PipelineLibrary` が shader 作成責務を持たないようにします。

### やること

次の直呼びを消します。

- `ShaderCompiler::CompileFromFile(...)`

代わりに次を使います。

- `ShaderCache::GetOrCreate(...)`

### 効果

- shader 側の失敗と PSO 側の失敗を分離できる

## Step 7-3. `CreatePipeline()` から root signature 生成を外す

### 目的

`PipelineLibrary` が root signature 生成責務を持たないようにします。

### やること

次の処理を削除します。

- `D3D12_ROOT_SIGNATURE_DESC` 構築
- `D3D12SerializeRootSignature(...)`
- `device->CreateRootSignature(...)`

代わりに次を使います。

- `RootSignatureCache::GetOrCreate(...)`

### 効果

- `PipelineLibrary` の責務が graphics PSO に近づく

## Step 7-4. `D3D12_GRAPHICS_PIPELINE_STATE_DESC` 構築に集中させる

### 目的

`PipelineLibrary` の中心処理を明確化します。

### やること

`CreatePipeline()` の中身を見たときに、
大半が `D3D12_GRAPHICS_PIPELINE_STATE_DESC` の組み立てになっている状態を目指します。

扱う主な項目:

- `pRootSignature`
- `VS`
- `PS`
- `BlendState`
- `RasterizerState`
- `DepthStencilState`
- `InputLayout`
- `PrimitiveTopologyType`
- `RTVFormats`

## Step 7-5. キャッシュ対象を PSO として明示する

### 目的

コードの意味を読みやすくします。

### やること

可能ならコメントや命名で次を明示します。

- `cache_` は Graphics PSO キャッシュである
- `PipelineDesc` は Graphics PSO のキーである
- `Pipeline` は PSO と bind に必要な関連情報を返す便宜構造である

### 例

- `pipelineStateCache_`
- `GraphicsPipelineDesc`
- `GraphicsPipelineCache`

ただし大きな rename は後回しでも構いません。

## Step 7-6. 旧責務を完全に消す

### 目的

分離の途中で残った古いコードを除去します。

### よく残りやすいもの

- 未使用の `RootParameterDesc`
- `RootSignatureType`
- root signature 用 helper
- shader compile flags の扱いが二重

### 確認項目

- `PipelineLibrary` 単体を読んだとき、root signature の内部構築コードが残っていないか
- shader compile のロジックが残っていないか

## 12. 途中実装で起きやすい設計ミス

## 12.1 `PipelineLibrary` がまだ RootSignature を作っている

これは最も多い混線です。  
`RootSignatureCache` を作っても、`PipelineLibrary` 側に旧コードが残っていると分離になりません。

## 12.2 `PipelineDesc` に実オブジェクトを入れてしまう

たとえばこれです。

- shader blob
- root signature object

これらを `PipelineDesc` に持たせると、キャッシュキーと実体が混ざります。

## 12.3 `Pipeline` が root signature の生成元を隠してしまう

`Pipeline` に root signature を含めるのは構いません。  
ただしそれを見て

- `PipelineLibrary` が root signature を作っている

と誤解しないよう、コメントや呼び出し経路を明確にする必要があります。

## 13. テスト観点

### 13.1 正常系

- 従来通り quad が描画されるか
- 同一 `PipelineDesc` でキャッシュヒットするか
- 異なる blend/depth/input layout で別 PSO になるか

### 13.2 キャッシュ観点

- 同じ shader と root signature を使っても PSO 設定が違えば別エントリになるか
- 完全一致なら同一 PSO を返すか

### 13.3 回帰観点

- `Material::Initialize()` が壊れていないか
- `Material::Bind()` が root signature と PSO を正しく使えているか
- `QuadRenderObject` の描画に回帰がないか

## 14. 命名改善の考え方

Step 7 の本質は rename ではありませんが、最終的には命名も寄せられます。

### 現状名

- `PipelineLibrary`

### 将来候補

- `GraphicsPipelineCache`
- `GraphicsPsoCache`

ただしこの変更は最後で構いません。  
先に責務を正す方が重要です。

## 15. 推奨実施順

最小リスクで進める順番は次です。

1. `ShaderCache` を通す
2. `RootSignatureCache` を通す
3. `CreatePipeline()` から旧 root signature 生成コードを削除
4. `PipelineDesc` を PSO キーとして見直す
5. `PipelineLibrary` のコメントと構造を PSO 中心に整理
6. 必要なら命名を見直す

## 16. まとめ

Step 7 のポイントは、

**`PipelineLibrary` を「何でも作るクラス」から「Graphics PSO を作ってキャッシュするクラス」へ寄せること**

です。

そのためには、

- shader は `ShaderCache`
- root signature は `RootSignatureCache`
- PSO は `PipelineLibrary`

という境界を崩さないことが重要です。

この整理が終わると、DX12 の主要概念ごとの責務がきれいに分かれ、
以後の保守や拡張がかなり楽になります。
