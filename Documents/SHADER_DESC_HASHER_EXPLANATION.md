# ShaderDescHasher 詳細解説

## 1. このドキュメントの目的

このドキュメントは、次のコードに出てくる `ShaderDescHasher` が何をしているのかを詳しく説明するためのものです。

```cpp
std::unordered_map<ShaderDesc, Microsoft::WRL::ComPtr<ID3DBlob>, ShaderDescHasher> cache_;
```

特に次の疑問に答えることを目的にしています。

- `ShaderDescHasher` はなぜ必要なのか
- 何を返す関数なのか
- `operator==` とどう関係するのか
- 間違えると何が起きるのか

## 2. まず結論

`ShaderDescHasher` は、`ShaderDesc` という構造体を  
`std::unordered_map` のキーとして使うための **ハッシュ関数オブジェクト** です。

簡単に言うと、

- `ShaderDesc` の内容を見て
- それを「検索しやすい数値」に変換する

ための部品です。

## 3. なぜ必要なのか

`std::unordered_map` は、中身を高速に探すために **ハッシュ値** を使います。

たとえば次のように、`int` や `std::string` をキーにするときは、標準ライブラリが最初からハッシュ方法を知っています。

```cpp
std::unordered_map<int, int> a;
std::unordered_map<std::string, int> b;
```

でも `ShaderDesc` は自作の構造体です。  
標準ライブラリは、

- `filePath`
- `entryPoint`
- `shaderModel`
- `compileFlags`

をどうやって1つのハッシュ値にまとめればいいか知りません。

そこで自分で

- 「この構造体はこうやってハッシュ値を作る」

というルールを教える必要があります。  
それが `ShaderDescHasher` です。

## 4. unordered_map が内部でやっていること

`unordered_map` はざっくり次の2段階でキーを扱います。

1. `hasher(key)` を呼んでハッシュ値を作る
2. 同じ候補があったら `operator==` で本当に同じキーか確認する

つまり、

- `ShaderDescHasher`
  - どの箱に入れるかを決める
- `operator==`
  - 本当に同じものか最終確認する

という役割分担です。

### 4.1 `unordered_map` は内部で `==` 比較もしているのか

結論から言うと、**はい、内部で比較も行います。**

ただし順番は、

1. まず `ShaderDescHasher` でハッシュ値を作る
2. そのハッシュ値に対応する候補を絞る
3. 候補同士を `==` で比較する

です。

つまり `unordered_map` は、

- ハッシュだけでキー一致を決めているわけではない
- 最後は「本当に同じキーか」を比較で確認している

という動きをします。

たとえば `cache_.find(desc)` を呼んだとき、内部では概念的に次のようなことが起きています。

```cpp
size_t hash = ShaderDescHasher{}(desc);
auto& bucket = table[hash];

for (const auto& entry : bucket)
{
    if (entry.key == desc)
    {
        return entry;
    }
}
```

もちろん実際の標準ライブラリ実装はもっと複雑ですが、理解としてはこれで十分です。

重要なのは次の点です。

- `ShaderDescHasher` は候補を素早く絞るために使われる
- `operator==` は最終的な一致判定に使われる

なので、

```cpp
std::unordered_map<ShaderDesc, Microsoft::WRL::ComPtr<ID3DBlob>, ShaderDescHasher> cache_;
```

という宣言は、`ShaderDescHasher` を指定しているだけでなく、キー型 `ShaderDesc` 側の等値比較も前提に動いています。

このドキュメントでは `operator==` を前提に説明していますが、正確には `unordered_map` の第4テンプレート引数には等値比較関数を明示することもできます。

例:

```cpp
std::unordered_map<ShaderDesc, Value, ShaderDescHasher, ShaderDescEqual>
```

ただし通常は、キー型に `operator==` を実装しておけば十分です。

## 5. ShaderDescHasher が必要な理由をたとえる

図書館の本を探す場面で考えると分かりやすいです。

- `ShaderDescHasher`
  - 本をどの棚に置くか決めるルール
- `operator==`
  - 棚の中で「本当にこの本か」を確認する作業

棚分けが雑でも検索はできますが遅くなります。  
棚分けが間違っていると、探しにくくなったり、意図しない衝突が増えたりします。

## 6. ShaderDesc とは何か

たとえば `ShaderDesc` が次のような構造体だとします。

```cpp
struct ShaderDesc
{
    std::wstring filePath;
    std::string entryPoint;
    std::string shaderModel;
    UINT compileFlags = 0;

    bool operator==(const ShaderDesc& other) const;
};
```

このとき、同じシェーダーとみなす条件は通常こうです。

- ファイルパスが同じ
- エントリポイントが同じ
- シェーダーモデルが同じ
- コンパイルフラグが同じ

これを `operator==` が定義します。

## 7. ShaderDescHasher の役割

`ShaderDescHasher` は、上の4項目を使って `size_t` 型の数値を返します。

典型的には次のような形です。

```cpp
struct ShaderDescHasher
{
    size_t operator()(const ShaderDesc& desc) const;
};
```

ここで大事なのは、`ShaderDescHasher` は **真偽値を返すものではない** ということです。  
返すのは「比較結果」ではなく「ハッシュ値」です。

## 8. 実装の基本形

もっとも基本的な実装イメージは次です。

```cpp
struct ShaderDescHasher
{
    size_t operator()(const ShaderDesc& desc) const
    {
        size_t seed = 0;

        HashCombine(seed, std::hash<std::wstring>{}(desc.filePath));
        HashCombine(seed, std::hash<std::string>{}(desc.entryPoint));
        HashCombine(seed, std::hash<std::string>{}(desc.shaderModel));
        HashCombine(seed, std::hash<UINT>{}(desc.compileFlags));

        return seed;
    }
};
```

ここでやっていることは、

- 各メンバを個別にハッシュする
- それらを1つの値に混ぜ合わせる

だけです。

## 9. HashCombine とは何か

複数の項目を1つのハッシュ値にまとめるための補助関数です。

よくある形は次のようなものです。

```cpp
inline void HashCombine(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
```

意味は厳密に全部覚えなくて大丈夫です。  
初心者向けには、次の理解で十分です。

- ただ足し算するだけだと偏りやすい
- 少し混ぜる処理を入れて、分布を良くしている

## 10. operator== と必ず整合していないといけない

ここが一番重要です。

`unordered_map` のキーでは、次のルールを守る必要があります。

**`operator==` で等しい2つの値は、必ず同じハッシュ値を返さなければならない**

たとえば、

```cpp
ShaderDesc a{L"A.hlsl", "Main", "vs_5_0", 1};
ShaderDesc b{L"A.hlsl", "Main", "vs_5_0", 1};
```

この2つが `operator==` で等しいなら、

```cpp
ShaderDescHasher{}(a) == ShaderDescHasher{}(b)
```

である必要があります。

## 10.1 逆は必須ではない

逆に、

- ハッシュ値が同じ
- でも `operator==` では違う

ことはあり得ます。これは **ハッシュ衝突** と呼ばれます。

例:

- 別の `ShaderDesc`
- たまたま同じハッシュ値になった

これは許容されます。  
その場合、`unordered_map` は最後に `operator==` で区別します。

## 11. よくある間違い

## 11.1 hasher に使う項目が足りない

たとえば `compileFlags` を `operator==` では比較しているのに、hasher に入れないケースです。

```cpp
bool ShaderDesc::operator==(const ShaderDesc& other) const
{
    return filePath == other.filePath &&
           entryPoint == other.entryPoint &&
           shaderModel == other.shaderModel &&
           compileFlags == other.compileFlags;
}
```

なのに hasher がこうだと危険です。

```cpp
size_t operator()(const ShaderDesc& desc) const
{
    size_t seed = 0;
    HashCombine(seed, std::hash<std::wstring>{}(desc.filePath));
    HashCombine(seed, std::hash<std::string>{}(desc.entryPoint));
    HashCombine(seed, std::hash<std::string>{}(desc.shaderModel));
    return seed;
}
```

これは整合性を壊すわけではありませんが、衝突が増えやすくなります。  
結果としてキャッシュ性能が悪くなります。

## 11.2 operator== と hasher の基準がずれる

たとえば `operator==` では `compileFlags` を比較しないのに、hasher では含めるケースです。

これは設計としてかなり危険です。

なぜなら、

- 等しいはずのキーなのに
- ハッシュ値だけ違う

という不整合が起きる可能性があるからです。

## 11.3 パスの正規化を考えない

たとえば次の2つは文字列としては違います。

- `L"BasicVertexShader.hlsl"`
- `L".\\BasicVertexShader.hlsl"`

でも意味としては同じファイルかもしれません。

この場合、`ShaderDescHasher` だけの問題ではなく、`ShaderDesc` の値を作る側で

- パスの正規化
- 大文字小文字の扱い

をそろえる必要があります。

## 12. 実際の map 宣言の意味を分解する

元の宣言はこれです。

```cpp
std::unordered_map<ShaderDesc, Microsoft::WRL::ComPtr<ID3DBlob>, ShaderDescHasher> cache_;
```

意味は次の通りです。

- キー: `ShaderDesc`
- 値: `ComPtr<ID3DBlob>`
- ハッシュ方法: `ShaderDescHasher`

つまり、

「このシェーダー条件に対応するコンパイル済み blob はこれ」

を覚える辞書です。

## 13. なぜ std::map ではなく unordered_map なのか

`std::map` は木構造です。  
`std::unordered_map` はハッシュベースです。

キャッシュ用途では通常こちらを使います。

- 完全な順序は不要
- 高速検索がほしい

その代わり、`unordered_map` では hasher が必要になります。

## 14. ShaderDescHasher の実装例

より実用的な例を示します。

```cpp
struct ShaderDesc
{
    std::wstring filePath;
    std::string entryPoint;
    std::string shaderModel;
    UINT compileFlags = 0;

    bool operator==(const ShaderDesc& other) const
    {
        return filePath == other.filePath &&
               entryPoint == other.entryPoint &&
               shaderModel == other.shaderModel &&
               compileFlags == other.compileFlags;
    }
};

inline void HashCombine(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct ShaderDescHasher
{
    size_t operator()(const ShaderDesc& desc) const
    {
        size_t seed = 0;
        HashCombine(seed, std::hash<std::wstring>{}(desc.filePath));
        HashCombine(seed, std::hash<std::string>{}(desc.entryPoint));
        HashCombine(seed, std::hash<std::string>{}(desc.shaderModel));
        HashCombine(seed, std::hash<UINT>{}(desc.compileFlags));
        return seed;
    }
};
```

この形なら、初心者でも追いやすく、実用上も十分です。

## 15. どこまで気にすべきか

初心者がまず押さえるべきポイントは3つだけです。

1. `ShaderDescHasher` は `ShaderDesc` を数値に変える係
2. `unordered_map` のキーに自作構造体を使うなら必要
3. `operator==` と同じ基準を使うことが重要

これだけ理解できれば十分スタートできます。

## 16. 実装チェックリスト

`ShaderDescHasher` を書くときは、次を確認します。

- `ShaderDesc` に `operator==` があるか
- `operator==` で比較する項目を hasher にも反映しているか
- `filePath` / `entryPoint` / `shaderModel` / `compileFlags` を入れているか
- `HashCombine` を使っているか
- map のキーとして使う値が正規化されているか

## 17. まとめ

`ShaderDescHasher` は難しい特別な概念ではなく、

**`ShaderDesc` を `unordered_map` のキーにするための「数値化ルール」**

です。

理解のポイントは次の1文です。

**`operator==` が「同じかどうか」を決め、`ShaderDescHasher` が「どこを探すか」を決める。**

この2つがそろって初めて、`ShaderCache` の辞書が正しく動きます。
