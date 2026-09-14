[CmdletBinding()]
param([string]$Version)

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

$stageRoot = Join-Path $snapshotRoot 'staging/github-source'
$fullStage = [System.IO.Path]::GetFullPath($stageRoot).TrimEnd('\')
if (-not $fullStage.StartsWith($snapshotRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "GitHub source stage escaped the snapshot root: $fullStage"
}
if (Test-Path -LiteralPath $fullStage) {
    Remove-Item -LiteralPath $fullStage -Recurse -Force
}
New-Item -ItemType Directory -Path $fullStage -Force | Out-Null

function Copy-PublicSourceFile([string]$RelativePath) {
    $source = Join-Path $snapshotRoot ($RelativePath -replace '/', '\')
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required public source file is missing: $RelativePath"
    }
    $destination = Join-Path $fullStage ($RelativePath -replace '/', '\')
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

$manifest = Get-WhereaboutsPublicSourceManifest
foreach ($file in $manifest.ExactFiles) {
    Copy-PublicSourceFile $file
}
foreach ($directory in $manifest.RecursiveDirectories) {
    $sourceDirectory = Join-Path $snapshotRoot ($directory -replace '/', '\')
    if (-not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
        throw "Required public source directory is missing: $directory"
    }
    foreach ($file in Get-ChildItem -LiteralPath $sourceDirectory -Recurse -File) {
        $relative = $file.FullName.Substring($snapshotRoot.Length + 1).Replace('\', '/')
        Copy-PublicSourceFile $relative
    }
}

Write-Output "GITHUB_SOURCE_STAGE=$fullStage"
