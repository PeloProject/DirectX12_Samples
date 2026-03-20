# ShaderCache 分離詳細ガイド

## 1. このドキュメントの目的

このドキュメントは、`PipelineLibrary` 改善手順のうち

- Step 5. `ShaderCache` を分離する

を、実装できる粒度まで詳しく説明するためのものです。

対象読者は次の人を想定しています。

- `PipelineLibrary` は読んだが、なぜ `ShaderCache` を分けるのかまだ腹落ちしていない人
- これから実際にコードを触る人
- DX12 の PSO とシェーダーの責務の違いを整理したい人

## 2. まず結論

`ShaderCache` を分離する理由は単純です。

**シェーダーのコンパイル結果の再利用** と  
**PSO の生成・再利用** は、似ているようで別の責務だからです。

今の `PipelineLibrary` は次を一度にやっています。

- HLSL ファイルを読む
- シェーダーをコンパイルする
- Root Signature を作る
- Graphics PSO を作る
- それをキャッシュする

この状態だと「PSO キャッシュ」と「シェーダーキャッシュ」が混ざっています。  
そのため、責務が広く、失敗時の切り分けもしづらく、今後の拡張にも不利です。

`ShaderCache` を分けると、次のように責務が整理されます。

- `ShaderCache`
  - シェーダーバイトコードを返す
- `PipelineLibrary`
  - そのバイトコードを使って PSO を作る

## 3. 現状の問題点

## 3.1 いま何が起きているか

`PipelineLibrary::CreatePipeline()` の中で、頂点シェーダーとピクセルシェーダーを毎回この流れで取得しています。

1. `ShaderCompiler::CompileFromFile()` を呼ぶ
2. VS の blob を得る
3. PS の blob を得る
4. その blob を使って PSO を作る

この設計だと、たとえば次のケースで無駄が発生します。

- 同じ VS/PS を使うマテリアルが複数ある
- Root Signature や Blend 設定だけが違う
- PSO は別々に必要だが、シェーダーのコンパイル結果は同じ

それでも毎回 `CompileFromFile()` を通る設計になっています。

## 3.2 なぜ問題か

問題は次の通りです。

- 同じシェーダーを何度もコンパイルする可能性がある
- PSO 生成失敗とシェーダーコンパイル失敗が同じクラスに混在する
- 再利用単位が粗い
- 将来的にシェーダー差し替えやホットリロードを入れにくい

## 4. ShaderCache を分けると何が良くなるか

### 4.1 再利用単位が正しくなる

PSO とシェーダーは別物です。

- シェーダー:
  - HLSL のコンパイル結果
- PSO:
  - シェーダーに加えて、入力レイアウト、ブレンド、深度、トポロジなどを含む描画設定の完成体

シェーダーが同じでも、PSO は複数あり得ます。  
なので、シェーダーだけを別キャッシュにした方が自然です。

### 4.2 失敗原因を切り分けやすくなる

`ShaderCache` 導入後は、失敗箇所が2段階で見られます。

- シェーダー取得失敗
  - ファイルがない
  - entry point がない
  - shader model が違う
- PSO 作成失敗
  - input layout 不整合
  - root signature 不整合
  - フォーマットや state の不整合

今より原因が追いやすくなります。

### 4.3 将来の拡張に強くなる

`ShaderCache` を分けておくと、今後次のような機能を入れやすくなります。

- シェーダーホットリロード
- シェーダーコンパイル統計
- コンパイル済み blob の使い回し
- shader include の管理
- compute shader 用の別経路

## 5. 分離後の責務設計

理想的な責務分担は次の通りです。

### ShaderCompiler

低レベル部品です。

責務:

- 指定されたファイルをコンパイルする
- `ID3DBlob` を返す
- コンパイルエラーを返す

これは「毎回コンパイルするだけ」の素朴な部品として残します。

### ShaderCache

新しく作る中間層です。

責務:

- コンパイル済みシェーダーをキャッシュする
- キーに一致する blob を返す
- なければ `ShaderCompiler` を呼んで生成する

### PipelineLibrary

責務:

- ShaderCache からシェーダー blob を受け取る
- Root Signature を作る
- Graphics PSO を作る
- PSO をキャッシュする

## 6. 新しく追加するクラス案

## 6.1 ファイル構成案

新規追加候補:

- `ApplicationDLL/Renderer/ShaderCache.h`
- `ApplicationDLL/Renderer/ShaderCache.cpp`

既存変更候補:

- `ApplicationDLL/Renderer/PipelineLibrary.cpp`
- `ApplicationDLL/Renderer/PipelineLibrary.h`

## 6.2 基本インターフェース案

`ShaderCache` の最小インターフェースは次のような形が考えられます。

```cpp
class ShaderCache final
{
public:
    struct ShaderDesc
    {
        std::wstring filePath;
        std::string entryPoint;
        std::string shaderModel;
        UINT compileFlags = 0;

        bool operator==(const ShaderDesc& other) const;
    };

    HRESULT GetOrCreate(
        const ShaderDesc& desc,
        Microsoft::WRL::ComPtr<ID3DBlob>* outBlob);

    void Clear();
};
```

返り値は `HRESULT` のままで構いません。  
今のコードベースと整合しやすいからです。

## 7. キャッシュキー設計

`ShaderCache` の品質はキー設計で決まります。  
最低限、次をキーに含める必要があります。

- shader file path
- entry point
- shader model
- compile flags

理由は次の通りです。

- 同じファイルでも entry point が違えば別シェーダー
- 同じ entry point でも `vs_5_0` と `ps_5_0` は別物
- compile flags が違うと生成物が変わる可能性がある

## 7.1 例

次の2つは別キーであるべきです。

- `BasicVertexShader.hlsl`, `BasicVS`, `vs_5_0`, Debug flags
- `BasicVertexShader.hlsl`, `BasicVS`, `vs_5_0`, Release flags

## 7.2 将来追加を検討できる項目

必要になれば次もキー候補です。

- include path 設定
- macro 定義
- source file write time

ただし最初から入れすぎると複雑になるため、初期版では不要です。

## 8. 保存する値

値として保存するのは基本的に `ID3DBlob` です。

たとえば次のような形です。

```cpp
std::unordered_map<ShaderDesc, Microsoft::WRL::ComPtr<ID3DBlob>, ShaderDescHasher> cache_;
```

必要なら将来、次の情報も持てます。

- コンパイル元パス
- 最終アクセス時刻
- ログ用の説明文字列

ですが、初期版では blob だけで十分です。

## 9. 実装ステップ詳細

## Step 5-1. ShaderDesc を定義する

### 目的

シェーダーキャッシュの最小キー型を作ることです。

### 実施内容

- `ShaderCache::ShaderDesc` を定義
- `operator==` を実装
- hasher を実装

### 注意

`PipelineDesc` の一部を流用してもよいですが、PSO 専用設定を混ぜないことが重要です。

## Step 5-2. ShaderCache::GetOrCreate() を実装する

### 目的

要求されたシェーダー blob を返す本体を作ることです。

### 処理の流れ

1. 引数チェック
2. mutex を取ってキャッシュ検索
3. あれば返す
4. なければ `ShaderCompiler::CompileFromFile()` を呼ぶ
5. 成功したらキャッシュに格納
6. blob を返す

### 擬似コード

```cpp
HRESULT ShaderCache::GetOrCreate(const ShaderDesc& desc, ComPtr<ID3DBlob>* outBlob)
{
    if (outBlob == nullptr)
    {
        return E_INVALIDARG;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(desc);
        if (it != cache_.end())
        {
            *outBlob = it->second;
            return S_OK;
        }
    }

    ComPtr<ID3DBlob> compiledBlob;
    HRESULT hr = ShaderCompiler::CompileFromFile(
        desc.filePath.c_str(),
        desc.entryPoint.c_str(),
        desc.shaderModel.c_str(),
        desc.compileFlags,
        compiledBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[desc] = compiledBlob;
    }

    *outBlob = compiledBlob;
    return S_OK;
}
```

## Step 5-3. PipelineLibrary の CompileFromFile 呼び出しを置き換える

### 目的

`PipelineLibrary` からシェーダーキャッシュを利用するように変更します。

### 現状

`PipelineLibrary::CreatePipeline()` の中で次を直接呼んでいます。

- VS 用 `ShaderCompiler::CompileFromFile()`
- PS 用 `ShaderCompiler::CompileFromFile()`

### 変更後

次のように変えます。

- VS は `ShaderCache::GetOrCreate(vsDesc, &vertexShaderBlob)`
- PS は `ShaderCache::GetOrCreate(psDesc, &pixelShaderBlob)`

### 効果

- `PipelineLibrary` が「コンパイルする人」ではなく「必要なシェーダーを受け取る人」になります

## Step 5-4. ShaderCache の所有場所を決める

### 選択肢 A. PipelineLibrary の内部メンバとして持つ

利点:

- 実装が簡単
- 変更範囲が小さい

弱点:

- 将来 RootSignatureCache などと並べると責務境界が少し曖昧

### 選択肢 B. グローバルまたは上位層で共有する

利点:

- 複数の pipeline library から共有しやすい
- 責務がはっきりする

弱点:

- 初期導入の変更量が増える

### 推奨

最初の導入は **選択肢 A** が安全です。  
まず `PipelineLibrary` の内部実装として `ShaderCache` を持ち、安定後に外へ出す方がよいです。

## 10. 導入後にコードがどう読みやすくなるか

導入前:

- `PipelineLibrary` を読むと
  - コンパイル
  - Root Signature
  - PSO
  - キャッシュ
  が全部出てくる

導入後:

- `PipelineLibrary` は
  - 必要な VS を取る
  - 必要な PS を取る
  - Root Signature を作る
  - PSO を作る
  に整理される

これだけでもかなり見通しが良くなります。

## 11. 失敗時ログの改善案

`ShaderCache` を分けるなら、ここでログも改善すると効果が高いです。

### 追加するとよいログ

- shader file path
- entry point
- shader model
- compile flags
- cache hit / miss

### 例

```cpp
LOG_DEBUG("ShaderCache miss: file=%ls entry=%s model=%s flags=0x%X",
    desc.filePath.c_str(),
    desc.entryPoint.c_str(),
    desc.shaderModel.c_str(),
    desc.compileFlags);
```

こうしておくと、PSO 以前の段階で何が起きたか把握しやすくなります。

## 12. テスト観点

`ShaderCache` 分離後は、最低限次を確認します。

### 12.1 正常系

- 初回描画で正常に shader がコンパイルされるか
- 同じ shader を使う次回初期化でキャッシュヒットするか
- quad が従来通り描画されるか

### 12.2 異常系

- 存在しない shader path で分かりやすく失敗するか
- 間違った entry point で失敗するか
- shader model 不一致で失敗するか

### 12.3 回帰

- `Material::Initialize()` が壊れていないか
- `QuadRenderObject::InitializeMaterial()` が従来通り動くか
- PSO 作成失敗時の挙動が変わりすぎていないか

## 13. 実装時の注意点

### 13.1 最初からホットリロードまで入れない

`ShaderCache` を分けるとホットリロードもやりたくなりますが、最初はやらない方が安全です。  
まずは「同じキーなら再利用できる」状態を作るべきです。

### 13.2 ファイル更新検知は後回しでよい

最初の `ShaderCache` は、起動中固定のキャッシュで十分です。  
ファイル更新時に自動無効化する仕組みは後で追加できます。

### 13.3 compile flags をキーに含め忘れない

ここを落とすと Debug/Release や設定差分で不正な再利用が起きる可能性があります。

### 13.4 キャッシュ参照と生成の二重生成は初期版では許容してよい

厳密には並列実行時に同じ shader を二重コンパイルする可能性があります。  
ただし今のコードベースでは、最初から in-flight 制御まで入れなくても構いません。

## 14. 段階的導入プラン

`ShaderCache` 分離は次の小ステップで入れると安全です。

### 第1段階

- `ShaderCache` クラスを追加
- まだ `PipelineLibrary` では使わない
- 単体でビルドを通す

### 第2段階

- VS だけ `ShaderCache` 経由にする
- 挙動確認する

### 第3段階

- PS も `ShaderCache` 経由にする
- 既存描画確認する

### 第4段階

- ログと統計を追加する
- キャッシュヒット確認する

この順で入れると、不具合時にどこで壊れたか追いやすいです。

## 15. 最終イメージ

最終的には、`PipelineLibrary::CreatePipeline()` は次のような責務だけを持つ形が望ましいです。

1. VS blob を取得
2. PS blob を取得
3. Root Signature を作る
4. Graphics PSO を作る
5. 結果を返す

このとき「1 と 2」は `ShaderCache` が担当し、`PipelineLibrary` 本体は DX12 パイプライン構築に集中できます。

## 16. まとめ

`ShaderCache` 分離は、単なるコード整理ではありません。  
これは「シェーダー」と「PSO」の再利用単位を正しく分けるための改善です。

理解のポイントは次の1文です。

**同じシェーダーは何度も使えるが、同じ PSO とは限らない。だからシェーダーキャッシュは PSO キャッシュから分ける。**

この考え方を軸に実装すると、`PipelineLibrary` の責務が自然に整理されます。
