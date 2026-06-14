# generate_pdf_html.ps1
# 詳細設計のMarkdownファイルをHTML(印刷用)に変換します。
# ブラウザで開いてCtrl+P → PDFに保存 でPDFを作成できます。

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$outputFile = Join-Path $scriptDir "詳細設計_印刷用.html"

# Get-ChildItem で取得することで日本語ファイル名のエンコーディング問題を回避
$mdFiles = Get-ChildItem -Path $scriptDir -Filter "*.md" |
           Sort-Object Name |
           Select-Object -ExpandProperty Name

# ---- ヘルパー関数 ----

function Escape-Html([string]$text) {
    $text = $text.Replace('&', '&amp;')
    $text = $text.Replace('<', '&lt;')
    $text = $text.Replace('>', '&gt;')
    $text = $text.Replace('"', '&quot;')
    return $text
}

function Apply-Inline([string]$text) {
    $text = Escape-Html $text
    $text = [regex]::Replace($text, '\*\*(.+?)\*\*',   '<strong>$1</strong>')
    $text = [regex]::Replace($text, '\*([^\*\n]+?)\*', '<em>$1</em>')
    $text = [regex]::Replace($text, '`([^`]+)`',        '<code>$1</code>')
    $text = [regex]::Replace($text, '\[([^\]]+)\]\(([^\)]+)\)', '<a href="$2">$1</a>')
    return $text
}

function Convert-MdToHtml([string]$content, [string]$filename) {
    $lines       = $content -split "`r?`n"
    $sb          = New-Object System.Text.StringBuilder
    $state       = 'normal'   # normal / plantuml / code
    $buf         = New-Object System.Text.StringBuilder
    $inList      = $false
    $inTable     = $false
    $tableIsFirst = $true

    foreach ($line in $lines) {

        # ===== PlantUML ブロック内 =====
        if ($state -eq 'plantuml') {
            if ($line -match '^\s*```\s*$') {
                $src     = $buf.ToString().TrimEnd("`r`n")
                $escaped = Escape-Html $src
                [void]$sb.AppendLine("<div class='puml'><pre class='puml-src'>$escaped</pre><div class='puml-msg'>⏳ 図を読み込み中...</div></div>")
                $buf.Clear(); $state = 'normal'
            } else {
                [void]$buf.AppendLine($line)
            }
            continue
        }

        # ===== コードブロック内 =====
        if ($state -eq 'code') {
            if ($line -match '^\s*```\s*$') {
                $code = Escape-Html $buf.ToString().TrimEnd("`r`n")
                [void]$sb.AppendLine("<pre><code>$code</code></pre>")
                $buf.Clear(); $state = 'normal'
            } else {
                [void]$buf.AppendLine($line)
            }
            continue
        }

        # ===== フェンス開始 =====
        if ($line -match '^\s*```plantuml\s*$') {
            if ($inList)  { [void]$sb.AppendLine('</ul>');    $inList  = $false }
            if ($inTable) { [void]$sb.AppendLine('</table>'); $inTable = $false }
            $buf.Clear(); $state = 'plantuml'; continue
        }
        if ($line -match '^\s*```(\w*)\s*$') {
            if ($inList)  { [void]$sb.AppendLine('</ul>');    $inList  = $false }
            if ($inTable) { [void]$sb.AppendLine('</table>'); $inTable = $false }
            $buf.Clear(); $state = 'code'; continue
        }

        # ===== 見出し =====
        if ($line -match '^(#{1,6})\s+(.+)$') {
            if ($inList)  { [void]$sb.AppendLine('</ul>');    $inList  = $false }
            if ($inTable) { [void]$sb.AppendLine('</table>'); $inTable = $false; $tableIsFirst = $true }
            $lv = $Matches[1].Length
            [void]$sb.AppendLine("<h$lv>$(Apply-Inline $Matches[2])</h$lv>")
            continue
        }

        # ===== 水平線 =====
        if ($line -match '^---+\s*$') {
            if ($inList)  { [void]$sb.AppendLine('</ul>');    $inList  = $false }
            if ($inTable) { [void]$sb.AppendLine('</table>'); $inTable = $false; $tableIsFirst = $true }
            [void]$sb.AppendLine('<hr>')
            continue
        }

        # ===== テーブル =====
        if ($line -match '^\|') {
            if ($inList) { [void]$sb.AppendLine('</ul>'); $inList = $false }
            # セパレーター行はスキップ
            if ($line -match '^\|[\s\-:]+(\|[\s\-:]+)+\|?\s*$') { continue }
            if (-not $inTable) {
                [void]$sb.AppendLine('<table>'); $inTable = $true; $tableIsFirst = $true
            }
            $tag   = if ($tableIsFirst) { 'th' } else { 'td' }
            $cells = $line -split '\|' | Where-Object { $_ -ne $null -and $_.Trim() -ne '' }
            [void]$sb.Append('<tr>')
            foreach ($c in $cells) { [void]$sb.Append("<$tag>$(Apply-Inline $c.Trim())</$tag>") }
            [void]$sb.AppendLine('</tr>')
            $tableIsFirst = $false
            continue
        } elseif ($inTable) {
            [void]$sb.AppendLine('</table>'); $inTable = $false; $tableIsFirst = $true
        }

        # ===== 箇条書き =====
        if ($line -match '^\s*[-*]\s+(.+)$') {
            if (-not $inList) { [void]$sb.AppendLine('<ul>'); $inList = $true }
            [void]$sb.AppendLine("<li>$(Apply-Inline $Matches[1])</li>")
            continue
        }
        if ($line -match '^\s*(\d+)\.\s+(.+)$') {
            if ($inList) { [void]$sb.AppendLine('</ul>'); $inList = $false }
            [void]$sb.AppendLine("<p class='ol-item'>$($Matches[1]). $(Apply-Inline $Matches[2])</p>")
            continue
        }

        # ===== 空行 =====
        if ($line.Trim() -eq '') {
            if ($inList) { [void]$sb.AppendLine('</ul>'); $inList = $false }
            continue
        }

        # ===== 通常行 =====
        if (-not $inList) {
            [void]$sb.AppendLine("<p>$(Apply-Inline $line)</p>")
        }
    }

    if ($inList)  { [void]$sb.AppendLine('</ul>') }
    if ($inTable) { [void]$sb.AppendLine('</table>') }

    return $sb.ToString()
}

# ---- ドキュメント本文を生成 ----

Write-Host "HTMLファイルを生成中..."
$body = New-Object System.Text.StringBuilder

foreach ($file in $mdFiles) {
    $path = Join-Path $scriptDir $file
    if (-not (Test-Path $path)) { Write-Warning "見つかりません: $path"; continue }
    $raw  = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    $html = Convert-MdToHtml $raw $file
    $name = [System.IO.Path]::GetFileNameWithoutExtension($file)
    [void]$body.AppendLine("<section class='doc'>")
    [void]$body.AppendLine("<div class='doc-header'>$name</div>")
    [void]$body.AppendLine($html)
    [void]$body.AppendLine("</section><div class='pb'></div>")
    Write-Host "  OK: $file"
}

# ---- HTML 組み立て ----
# (JavaScriptの $ はないため here-string 内の展開は問題なし)

$today     = Get-Date -Format 'yyyy-MM-dd'
$bodyHtml  = $body.ToString()

$fullHtml = @"
<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="UTF-8">
<title>DirectX12_Samples 詳細設計資料</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/pako/2.1.0/pako.min.js"></script>
<style>
*{box-sizing:border-box}
body{font-family:"Meiryo UI","Yu Gothic UI","MS Gothic",sans-serif;font-size:11pt;line-height:1.65;color:#222;margin:0;padding:0}
.doc{max-width:980px;margin:0 auto;padding:28px 36px}
.doc-header{font-size:8.5pt;color:#888;border-bottom:1px solid #ddd;padding-bottom:3px;margin-bottom:14px}
h1{font-size:17pt;color:#0D47A1;border-bottom:2.5px solid #1565C0;padding-bottom:5px;margin:22px 0 10px}
h2{font-size:13pt;color:#1565C0;border-bottom:1px solid #90CAF9;padding-bottom:3px;margin:18px 0 8px}
h3{font-size:11.5pt;color:#1976D2;margin:14px 0 6px}
h4{font-size:11pt;color:#1E88E5;margin:10px 0 4px}
h5,h6{font-size:10pt;color:#2196F3}
p{margin:5px 0}
.ol-item{margin-left:1.8em}
pre{background:#1e1e1e;color:#d4d4d4;padding:10px 14px;border-radius:5px;overflow-x:auto;font-size:8.5pt;font-family:"Consolas","Courier New",monospace;white-space:pre-wrap;word-break:break-all;margin:8px 0}
code{background:#f0f0f0;padding:1px 5px;border-radius:3px;font-family:"Consolas","Courier New",monospace;font-size:8.5pt}
pre code{background:none;padding:0;color:inherit}
table{border-collapse:collapse;width:100%;margin:10px 0;font-size:10pt}
th{background:#1565C0;color:#fff;padding:6px 10px;text-align:left;border:1px solid #0D47A1}
td{padding:5px 9px;border:1px solid #ccc}
tr:nth-child(even) td{background:#F5F5F5}
ul{margin:5px 0;padding-left:1.6em}
li{margin:3px 0}
hr{border:none;border-top:1px solid #ddd;margin:14px 0}
a{color:#1565C0}
strong{color:#111}
.puml{text-align:center;margin:14px 0;padding:8px;border:1px dashed #90CAF9;border-radius:5px;background:#fafafa;min-height:40px}
.puml-src{display:none}
.puml img{max-width:100%;height:auto;display:block;margin:0 auto}
.puml-msg{color:#888;font-size:9pt;padding:6px}
.pb{page-break-after:always;break-after:page;height:0}
.cover{height:100vh;display:flex;align-items:center;justify-content:center;flex-direction:column;text-align:center}
.cover h1{border:none;font-size:26pt;margin:0}
.cover h2{border:none;font-size:18pt;color:#444;font-weight:normal;margin:8px 0 0}
.cover p{color:#888;margin-top:30px}
@media print{
  .pb{page-break-after:always}
  body{font-size:10pt}
  pre{font-size:7.5pt}
  .cover{height:100vh}
}
@page{size:A4;margin:15mm}
</style>
</head>
<body>

<div class="cover">
  <h1>DirectX12_Samples</h1>
  <h2>詳細設計資料</h2>
  <p>作成日: $today</p>
</div>
<div class="pb"></div>

$bodyHtml

<script>
// PlantUML エンコーダ (raw deflate + カスタムBase64)
function pumlEncode(src){
  var s=unescape(encodeURIComponent(src));
  var b=new Uint8Array(s.length);
  for(var i=0;i<s.length;i++) b[i]=s.charCodeAt(i);
  var c=pako.deflateRaw(b,{level:9});
  var r='';
  for(var i=0;i<c.length;i+=3){
    var b1=c[i],b2=i+1<c.length?c[i+1]:0,b3=i+2<c.length?c[i+2]:0;
    r+=e6(b1>>2);r+=e6(((b1&3)<<4)|(b2>>4));
    r+=e6(((b2&15)<<2)|(b3>>6));r+=e6(b3&63);
  }
  return r;
}
function e6(b){
  b&=63;
  if(b<10) return String.fromCharCode(48+b);
  b-=10; if(b<26) return String.fromCharCode(65+b);
  b-=26; if(b<26) return String.fromCharCode(97+b);
  b-=26; return b===0?'-':'_';
}

window.addEventListener('load',function(){
  document.querySelectorAll('.puml').forEach(function(box){
    var pre=box.querySelector('.puml-src');
    if(!pre) return;
    var src=pre.textContent.trim();
    var encoded=pumlEncode(src);
    var img=document.createElement('img');
    img.alt='PlantUML Diagram';
    img.onload=function(){ var m=box.querySelector('.puml-msg'); if(m) m.style.display='none'; };
    img.onerror=function(){ var m=box.querySelector('.puml-msg'); if(m) m.textContent='図の読み込みに失敗しました (インターネット接続を確認してください)'; };
    img.src='https://www.plantuml.com/plantuml/png/'+encoded;
    box.insertBefore(img, box.querySelector('.puml-msg'));
  });
});
</script>
</body>
</html>
"@

[System.IO.File]::WriteAllText($outputFile, $fullHtml, (New-Object System.Text.UTF8Encoding $true))

Write-Host ""
Write-Host "生成完了: $outputFile"
Write-Host ""
Write-Host "使い方:"
Write-Host "  1. 上記のHTMLファイルを Chrome または Edge で開く"
Write-Host "  2. PlantUML図の読み込みを待つ (インターネット接続が必要)"
Write-Host "  3. Ctrl+P → 送信先:「PDFに保存」→「保存」"
Write-Host ""
Write-Host "印刷設定の推奨:"
Write-Host "  用紙サイズ: A4  /  余白: 最小  /  倍率: 80-90%"
