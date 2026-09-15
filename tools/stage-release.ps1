[CmdletBinding()]
param(
    [string]$Version,
    [switch]$CreateArchives,
    [string]$RuntimeDllPath,
    [switch]$ExperimentalVr
)

$ErrorActionPreference = 'Stop'
$snapshotRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
. (Join-Path $PSScriptRoot 'VersionContract.ps1')
$versionContract = Get-WhereaboutsVersionContract -ProjectRoot $snapshotRoot
$canonicalVersion = $versionContract.display
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $canonicalVersion
}
elseif ($Version -ne $canonicalVersion) {
    throw "Requested version '$Version' does not match canonical version '$canonicalVersion'."
}
if ($ExperimentalVr -and $versionContract.artifact -eq $versionContract.display) {
    throw 'ExperimentalVr requires a distinct canonical VR artifact label.'
}

$stagingRoot = Join-Path $snapshotRoot 'staging'
$runtimeStage = Join-Path $stagingRoot 'runtime'
$translationStage = Join-Path $stagingRoot 'translations'
$releaseRoot = Join-Path $snapshotRoot 'release'

function Reset-SafeStage([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    if (-not $full.StartsWith($snapshotRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Stage path escaped the snapshot root: $full"
    }
    if (Test-Path -LiteralPath $full) {
        Remove-Item -LiteralPath $full -Recurse -Force
    }
    New-Item -ItemType Directory -Path $full -Force | Out-Null
}

function Copy-ReleaseFile([string]$Source, [string]$DestinationRoot, [string]$RelativePath) {
    $sourcePath = if ([System.IO.Path]::IsPathRooted($Source)) {
        [System.IO.Path]::GetFullPath($Source)
    }
    else {
        Join-Path $snapshotRoot $Source
    }
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Required release file is missing: $Source"
    }
    $destination = Join-Path $DestinationRoot ($RelativePath -replace '/', '\')
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destination -Force
}

function Get-RelativeFiles([string]$Root) {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    @(Get-ChildItem -LiteralPath $rootPath -Recurse -File | ForEach-Object {
        $_.FullName.Substring($rootPath.Length + 1).Replace('\', '/')
    } | Sort-Object)
}

function New-DeterministicZip([string]$SourceRoot, [string]$ArchivePath) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    if (Test-Path -LiteralPath $ArchivePath) {
        Remove-Item -LiteralPath $ArchivePath -Force
    }
    $fileStream = [System.IO.File]::Open($ArchivePath, [System.IO.FileMode]::CreateNew)
    try {
        $archive = [System.IO.Compression.ZipArchive]::new(
            $fileStream,
            [System.IO.Compression.ZipArchiveMode]::Create,
            $false)
        try {
            foreach ($relative in Get-RelativeFiles $SourceRoot) {
                $entry = $archive.CreateEntry($relative, [System.IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = [System.DateTimeOffset]::new(
                    2020, 1, 1, 0, 0, 0, [System.TimeSpan]::Zero)
                $input = [System.IO.File]::OpenRead((Join-Path $SourceRoot ($relative -replace '/', '\')))
                try {
                    $output = $entry.Open()
                    try { $input.CopyTo($output) } finally { $output.Dispose() }
                }
                finally { $input.Dispose() }
            }
        }
        finally { $archive.Dispose() }
    }
    finally { $fileStream.Dispose() }
}

$runtimeDllSource = if ([string]::IsNullOrWhiteSpace($RuntimeDllPath)) {
    if ($ExperimentalVr) {
        'build/vs2022-vr-release/Release/Whereabouts.dll'
    } else {
        'build/vs2022-release/Release/Whereabouts.dll'
    }
}
else {
    [System.IO.Path]::GetFullPath($RuntimeDllPath)
}
$runtimeFiles = [ordered]@{
    'plugin/Whereabouts.esp' = 'Whereabouts.esp'
    $runtimeDllSource = 'SKSE/Plugins/Whereabouts.dll'
    'config/Whereabouts.ini' = 'SKSE/Plugins/Whereabouts.ini'
    'papyrus/Compiled/WhereaboutsAPI.pex' = 'Scripts/WhereaboutsAPI.pex'
    'papyrus/Compiled/WhereaboutsQuest.pex' = 'Scripts/WhereaboutsQuest.pex'
    'papyrus/Compiled/WhereaboutsTrackedAlias.pex' = 'Scripts/WhereaboutsTrackedAlias.pex'
    'papyrus/Compiled/WhereaboutsNative.pex' = 'Scripts/WhereaboutsNative.pex'
    'external/CommonLibSSE-NG/COPYING' = 'LICENSES/CommonLibSSE-NG-GPL-3.0.txt'
    'external/CommonLibSSE-NG/EXCEPTIONS.md' = 'LICENSES/CommonLibSSE-NG-EXCEPTIONS.md'
    'LICENSES/SKSE-Menu-Framework-API.txt' = 'LICENSES/SKSE-Menu-Framework-API.txt'
    'LICENSES/Whereabouts-Permissions.txt' = 'LICENSES/Whereabouts-Permissions.txt'
    'LICENSES/Third-Party-Notices.txt' = 'LICENSES/Third-Party-Notices.txt'
}
$languages = @('ENGLISH', 'FRENCH', 'ITALIAN', 'GERMAN', 'SPANISH', 'POLISH', 'CHINESE', 'RUSSIAN', 'JAPANESE')
foreach ($language in $languages) {
    $relative = "Interface/Translations/Whereabouts_$language.txt"
    $runtimeFiles[$relative] = $relative
}

Reset-SafeStage $runtimeStage
foreach ($entry in $runtimeFiles.GetEnumerator()) {
    Copy-ReleaseFile $entry.Key $runtimeStage $entry.Value
}

if (-not $ExperimentalVr) {
    Reset-SafeStage $translationStage
    foreach ($language in $languages | Where-Object { $_ -ne 'ENGLISH' }) {
        Copy-ReleaseFile `
            "translations/machine/Whereabouts_$language.txt" `
            $translationStage `
            "Interface/Translations/Whereabouts_$language.txt"
    }
}

if ($CreateArchives) {
    New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
    if ($ExperimentalVr) {
        Get-ChildItem -LiteralPath $releaseRoot -File -Filter "Whereabouts-$($versionContract.artifact)-*.zip" |
            Remove-Item -Force
        New-DeterministicZip $runtimeStage (Join-Path $releaseRoot "Whereabouts-$($versionContract.artifact)-Experimental-VR.zip")
    } else {
        New-DeterministicZip $runtimeStage (Join-Path $releaseRoot "Whereabouts-$Version-Main.zip")
        New-DeterministicZip $translationStage (Join-Path $releaseRoot "Whereabouts-$Version-Translations.zip")
    }
}

Write-Output "RUNTIME_STAGE=$runtimeStage"
if (-not $ExperimentalVr) { Write-Output "TRANSLATION_STAGE=$translationStage" }
