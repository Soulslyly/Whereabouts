[CmdletBinding()]
param(
    [string]$Version,
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = 'Stop'
$snapshotRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
. (Join-Path $PSScriptRoot 'VersionContract.ps1')
. (Join-Path $PSScriptRoot 'PublicSourceManifest.ps1')
$canonicalVersion = (Get-WhereaboutsVersionContract -ProjectRoot $snapshotRoot).display
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $canonicalVersion
}
elseif ($Version -ne $canonicalVersion) {
    throw "Requested version '$Version' does not match canonical version '$canonicalVersion'."
}

$sourceRootPath = [System.IO.Path]::GetFullPath($SourceRoot).TrimEnd('\')
if (-not (Test-Path -LiteralPath $sourceRootPath -PathType Container)) {
    throw "GitHub source root is missing: $sourceRootPath"
}

function Get-RelativeFiles([string]$Root) {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    @(Get-ChildItem -LiteralPath $rootPath -Recurse -File | ForEach-Object {
        $_.FullName.Substring($rootPath.Length + 1).Replace('\', '/')
    } | Sort-Object)
}

$manifest = Get-WhereaboutsPublicSourceManifest
$expected = [System.Collections.Generic.List[string]]::new()
foreach ($file in $manifest.ExactFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $snapshotRoot ($file -replace '/', '\')) -PathType Leaf)) {
        throw "Manifest points to a missing project file: $file"
    }
    $expected.Add($file)
}
foreach ($directory in $manifest.RecursiveDirectories) {
    $projectDirectory = Join-Path $snapshotRoot ($directory -replace '/', '\')
    foreach ($file in Get-ChildItem -LiteralPath $projectDirectory -Recurse -File) {
        $expected.Add($file.FullName.Substring($snapshotRoot.Length + 1).Replace('\', '/'))
    }
}
$expectedFiles = @($expected | Sort-Object -Unique)
$actualFiles = Get-RelativeFiles $sourceRootPath
if ([string]::Join("`n", $actualFiles) -ne [string]::Join("`n", $expectedFiles)) {
    $missing = @($expectedFiles | Where-Object { $_ -notin $actualFiles })
    $unexpected = @($actualFiles | Where-Object { $_ -notin $expectedFiles })
    throw "GitHub source inventory mismatch. Missing=[$($missing -join ', ')] Unexpected=[$($unexpected -join ', ')]"
}

$denied = $actualFiles | Where-Object {
    $_ -match '(^|/)\.[^/]+/' -or
    $_ -match '(^|/)(\.git|build|staging|release|bin|obj|\.deps|tests?|docs/superpowers)(/|$)' -or
    $_ -match '\.(dll|pex|esp|esm|esl|pdb|ilk|obj|lib|exp|log|cache)$' -or
    $_ -match '(^|/)(PLAN|STATE|DECISIONS|VALIDATION|CURRENT|WORKSPACE_OWNERSHIP)\.(md|txt)$'
}
if ($denied) {
    throw "GitHub source contains denied material: $($denied -join ', ')"
}

$absolutePathPattern = '(?im)(?<![a-z0-9+.-])[a-z]:[\\/](?![\\/])'
$disallowedMarkers = @(
    (-join @(67, 104, 97, 116, 71, 80, 84 | ForEach-Object { [char]$_ })),
    (-join @(79, 112, 101, 110, 65, 73 | ForEach-Object { [char]$_ })),
    (-join @(103, 101, 110, 101, 114, 97, 116, 101, 100, 32, 98, 121, 32, 65, 73 | ForEach-Object { [char]$_ }))
)
$textExtensions = @('.cs', '.cpp', '.h', '.ini', '.json', '.md', '.ps1', '.psc', '.txt', '.tsv')
foreach ($relative in $actualFiles) {
    $path = Join-Path $sourceRootPath ($relative -replace '/', '\')
    if ([System.IO.Path]::GetExtension($path).ToLowerInvariant() -notin $textExtensions -and
        [System.IO.Path]::GetFileName($path) -notin @('.gitignore', '.gitmodules')) {
        continue
    }
    $content = Get-Content -LiteralPath $path -Raw
    if ($null -eq $content) { $content = '' }
    if ([regex]::IsMatch($content, $absolutePathPattern)) {
        throw "GitHub source text contains an absolute local path: $relative"
    }
    foreach ($marker in $disallowedMarkers) {
        if ($content.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw "GitHub source text contains internal generated-content material: $relative"
        }
    }
}

$stagedContract = Get-Content -LiteralPath (Join-Path $sourceRootPath 'version.json') -Raw | ConvertFrom-Json
if ($stagedContract.display -ne $Version) {
    throw "GitHub source version.json does not match $Version"
}
if ((Get-Content -LiteralPath (Join-Path $sourceRootPath 'VERSION.txt') -TotalCount 1).Trim() -ne "Whereabouts $Version") {
    throw "GitHub source VERSION.txt does not match $Version"
}

Write-Output "GITHUB_SOURCE_FILES=$($actualFiles.Count)"
Write-Output 'RESULT=PASS'
