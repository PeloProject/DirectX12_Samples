[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectPath,

    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Normalize-RelPath {
    param([string]$PathValue)
    return ($PathValue -replace "/", "\\").Trim()
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Get-DefaultRootFilter {
    param([string]$ItemName)

    switch ($ItemName) {
        "ClCompile" { return "Source Files" }
        "ClInclude" { return "Header Files" }
        "ResourceCompile" { return "Resource Files" }
        "Image" { return "Resource Files" }
        "None" { return "Other Files" }
        default { return "Other Files" }
    }
}

$projectFullPath = [System.IO.Path]::GetFullPath($ProjectPath)
if (-not (Test-Path $projectFullPath)) {
    throw "Project not found: $projectFullPath"
}

if ([System.IO.Path]::GetExtension($projectFullPath).ToLowerInvariant() -eq ".filters") {
    $vcxprojPath = $projectFullPath.Substring(0, $projectFullPath.Length - ".filters".Length)
    $filtersPath = $projectFullPath
} else {
    $vcxprojPath = $projectFullPath
    $filtersPath = "$projectFullPath.filters"
}

if (-not (Test-Path $vcxprojPath)) {
    throw ".vcxproj not found: $vcxprojPath"
}

[xml]$projXml = Get-Content -Path $vcxprojPath -Raw -Encoding UTF8
$ns = New-Object System.Xml.XmlNamespaceManager($projXml.NameTable)
$ns.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

$fileItems = New-Object System.Collections.Generic.List[object]
$filters = New-Object System.Collections.Generic.HashSet[string]

$nodes = $projXml.SelectNodes("//msb:ItemGroup/*[@Include]", $ns)
foreach ($node in $nodes) {
    $itemName = $node.LocalName
    $include = Normalize-RelPath $node.Include
    if ([string]::IsNullOrWhiteSpace($include)) {
        continue
    }
    if ($include.IndexOfAny(@([char]'|', [char]'*', [char]'?', [char]'%')) -ge 0) {
        continue
    }

    $dir = [System.IO.Path]::GetDirectoryName($include)
    $root = Get-DefaultRootFilter -ItemName $itemName

    $filter = if ([string]::IsNullOrWhiteSpace($dir)) {
        $root
    } else {
        Normalize-RelPath (Join-Path $root $dir)
    }

    $parts = @($filter.Split("\\") | Where-Object { $_ -ne "" })
    for ($i = 0; $i -lt $parts.Count; $i++) {
        $filters.Add(($parts[0..$i] -join "\\")) | Out-Null
    }

    $fileItems.Add([pscustomobject]@{
            ItemName = $itemName
            Include  = $include
            Filter   = $filter
        })
}

$xmlSettings = New-Object System.Xml.XmlWriterSettings
$xmlSettings.Indent = $true
$xmlSettings.IndentChars = "  "
$xmlSettings.Encoding = [System.Text.UTF8Encoding]::new($false)
$sw = New-Object System.IO.StringWriter
$writer = [System.Xml.XmlWriter]::Create($sw, $xmlSettings)

$writer.WriteStartDocument()
$writer.WriteStartElement("Project", "http://schemas.microsoft.com/developer/msbuild/2003")
$writer.WriteAttributeString("ToolsVersion", "4.0")

$writer.WriteStartElement("ItemGroup")
foreach ($filterName in ($filters | Sort-Object)) {
    $writer.WriteStartElement("Filter")
    $writer.WriteAttributeString("Include", $filterName)
    $writer.WriteElementString("UniqueIdentifier", "{$([guid]::NewGuid().ToString())}")
    if ($filterName -eq "Source Files") {
        $writer.WriteElementString("Extensions", "cpp;c;cc;cxx;c++;cppm;ixx;def;odl;idl;hpj;bat;asm;asmx")
    } elseif ($filterName -eq "Header Files") {
        $writer.WriteElementString("Extensions", "h;hh;hpp;hxx;h++;hm;inl;inc;ipp;xsd")
    } elseif ($filterName -eq "Resource Files") {
        $writer.WriteElementString("Extensions", "rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx;tiff;tif;png;wav;mfcribbon-ms")
    }
    $writer.WriteEndElement()
}
$writer.WriteEndElement()

$grouped = $fileItems | Group-Object ItemName
foreach ($group in $grouped) {
    $writer.WriteStartElement("ItemGroup")
    foreach ($item in ($group.Group | Sort-Object Include)) {
        $writer.WriteStartElement($item.ItemName)
        $writer.WriteAttributeString("Include", $item.Include)
        $writer.WriteElementString("Filter", $item.Filter)
        $writer.WriteEndElement()
    }
    $writer.WriteEndElement()
}

$writer.WriteEndElement()
$writer.WriteEndDocument()
$writer.Flush()
$writer.Close()

$content = $sw.ToString()
if ($DryRun) {
    Write-Host "DryRun enabled. Generated filters content preview (head):"
    $content.Split([Environment]::NewLine) | Select-Object -First 30 | ForEach-Object { Write-Host $_ }
    exit 0
}

Write-Utf8NoBom -Path $filtersPath -Content $content
Write-Host "Updated: $filtersPath"
