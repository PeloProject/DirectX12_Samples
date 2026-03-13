[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectPath,

    [switch]$DryRun,

    [switch]$NoHeaderShim
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Normalize-RelPath {
    param([string]$PathValue)
    return ($PathValue -replace "/", "\\").Trim()
}

function Get-RelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $base = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $base.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $base += [System.IO.Path]::DirectorySeparatorChar
    }

    $target = [System.IO.Path]::GetFullPath($TargetPath)
    $baseUri = [System.Uri]::new($base)
    $targetUri = [System.Uri]::new($target)
    $relativeUri = $baseUri.MakeRelativeUri($targetUri)
    return [System.Uri]::UnescapeDataString($relativeUri.ToString()) -replace "/", "\\"
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

$projectFullPath = [System.IO.Path]::GetFullPath($ProjectPath)
if (-not (Test-Path $projectFullPath)) {
    throw "Project not found: $projectFullPath"
}

if ([System.IO.Path]::GetExtension($projectFullPath).ToLowerInvariant() -eq ".filters") {
    $filtersPath = $projectFullPath
    $vcxprojPath = $projectFullPath.Substring(0, $projectFullPath.Length - ".filters".Length)
} else {
    $vcxprojPath = $projectFullPath
    $filtersPath = "$projectFullPath.filters"
}

if (-not (Test-Path $vcxprojPath)) {
    throw ".vcxproj not found: $vcxprojPath"
}
if (-not (Test-Path $filtersPath)) {
    throw ".vcxproj.filters not found: $filtersPath"
}

$projectDir = Split-Path -Parent $vcxprojPath
[xml]$filtersXml = Get-Content -Path $filtersPath -Raw -Encoding UTF8
$ns = New-Object System.Xml.XmlNamespaceManager($filtersXml.NameTable)
$ns.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

$rootFilters = New-Object System.Collections.Generic.HashSet[string]
$rootFilterNodes = $filtersXml.SelectNodes("//msb:Filter[msb:Extensions]", $ns)
foreach ($rootNode in $rootFilterNodes) {
    if (-not [string]::IsNullOrWhiteSpace($rootNode.Include)) {
        $rootFilters.Add((Normalize-RelPath $rootNode.Include)) | Out-Null
    }
}

$itemNodes = $filtersXml.SelectNodes("//msb:ItemGroup/*[@Include][msb:Filter]", $ns)
$moves = New-Object System.Collections.Generic.List[object]
$destMap = @{}

foreach ($node in $itemNodes) {
    $oldRel = Normalize-RelPath $node.Include
    $filterNode = $node.SelectSingleNode("msb:Filter", $ns)
    if ($null -eq $filterNode) {
        continue
    }

    $filterPath = Normalize-RelPath $filterNode.InnerText
    if ([string]::IsNullOrWhiteSpace($filterPath)) {
        continue
    }

    $segments = @($filterPath.Split("\\") | Where-Object { $_ -ne "" })
    if ($segments.Count -eq 0) {
        continue
    }
    if ($rootFilters.Contains($segments[0])) {
        $segments = @($segments | Select-Object -Skip 1)
    }

    $targetDirRel = if ($segments.Count -gt 0) { ($segments -join "\\") } else { "" }
    $fileName = [System.IO.Path]::GetFileName($oldRel)
    if ([string]::IsNullOrWhiteSpace($fileName)) {
        continue
    }

    $newRel = if ([string]::IsNullOrWhiteSpace($targetDirRel)) {
        $fileName
    } else {
        Normalize-RelPath (Join-Path $targetDirRel $fileName)
    }

    if ($oldRel -eq $newRel) {
        continue
    }

    $absOld = [System.IO.Path]::GetFullPath((Join-Path $projectDir $oldRel))
    $absNew = [System.IO.Path]::GetFullPath((Join-Path $projectDir $newRel))

    if ($destMap.ContainsKey($newRel)) {
        throw "Conflict: multiple files map to '$newRel' ('$oldRel' and '$($destMap[$newRel])')."
    }
    $destMap[$newRel] = $oldRel

    $moves.Add([pscustomobject]@{
            OldRel = $oldRel
            NewRel = $newRel
            AbsOld = $absOld
            AbsNew = $absNew
            Ext    = [System.IO.Path]::GetExtension($oldRel).ToLowerInvariant()
        })
}

if ($moves.Count -eq 0) {
    Write-Host "No move needed: $vcxprojPath"
    exit 0
}

$moves = @($moves | Sort-Object { $_.OldRel.Length } -Descending)
$headerExt = @(".h", ".hh", ".hpp", ".hxx", ".inl", ".inc", ".ipp")

Write-Host "Project: $vcxprojPath"
Write-Host "Planned moves: $($moves.Count)"
foreach ($move in $moves) {
    Write-Host "  $($move.OldRel) -> $($move.NewRel)"
}

if ($DryRun) {
    Write-Host "DryRun enabled. No files were changed."
    exit 0
}

foreach ($move in $moves) {
    if (-not (Test-Path $move.AbsOld)) {
        Write-Warning "Missing source file. Skipped move: $($move.OldRel)"
        continue
    }

    $destDir = Split-Path -Parent $move.AbsNew
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    if ((Test-Path $move.AbsNew) -and ([System.IO.Path]::GetFullPath($move.AbsNew) -ne [System.IO.Path]::GetFullPath($move.AbsOld))) {
        throw "Destination already exists: $($move.NewRel)"
    }

    Move-Item -Path $move.AbsOld -Destination $move.AbsNew -Force

    if (-not $NoHeaderShim -and ($headerExt -contains $move.Ext)) {
        $oldDir = Split-Path -Parent $move.AbsOld
        if (-not (Test-Path $oldDir)) {
            New-Item -ItemType Directory -Path $oldDir -Force | Out-Null
        }

        $shimRel = (Get-RelativePath -BasePath $oldDir -TargetPath $move.AbsNew) -replace "\\", "/"
        $shim = @(
            "#pragma once",
            "#include `"$shimRel`""
        ) -join [Environment]::NewLine
        Write-Utf8NoBom -Path $move.AbsOld -Content $shim
    }
}

$filtersText = Get-Content -Path $filtersPath -Raw -Encoding UTF8
$vcxprojText = Get-Content -Path $vcxprojPath -Raw -Encoding UTF8

foreach ($move in $moves) {
    $oldPattern = "Include=""$($move.OldRel)"""
    $newValue = "Include=""$($move.NewRel)"""
    $filtersText = $filtersText.Replace($oldPattern, $newValue)
    $vcxprojText = $vcxprojText.Replace($oldPattern, $newValue)
}

Write-Utf8NoBom -Path $filtersPath -Content $filtersText
Write-Utf8NoBom -Path $vcxprojPath -Content $vcxprojText

Write-Host "Completed: moved files and updated project/filter includes."
