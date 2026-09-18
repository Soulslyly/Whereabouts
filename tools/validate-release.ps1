[CmdletBinding()]
param(
    [string]$Version,
    [switch]$RequireArchives,
    [switch]$RequireVrArchive,
    [string]$MenuFrameworkDll
)

$ErrorActionPreference = 'Stop'
$snapshotRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
. (Join-Path $PSScriptRoot 'VersionContract.ps1')
$canonicalVersion = (Get-WhereaboutsVersionContract -ProjectRoot $snapshotRoot).display
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $canonicalVersion
}
elseif ($Version -ne $canonicalVersion) {
    throw "Requested version '$Version' does not match canonical version '$canonicalVersion'."
}

if ($RequireArchives) {
    if ([string]::IsNullOrWhiteSpace($MenuFrameworkDll)) {
        throw 'MenuFrameworkDll is required for complete archive validation.'
    }
    & (Join-Path $snapshotRoot 'tools/validate-smf-binary.ps1') `
        -MenuFrameworkDll $MenuFrameworkDll `
        -ProjectRoot $snapshotRoot
}
if ($RequireVrArchive -and -not $RequireArchives) {
    throw 'RequireVrArchive also requires RequireArchives.'
}

$runtimeStage = Join-Path $snapshotRoot 'staging/runtime'
$vrStage = Join-Path $snapshotRoot 'staging/vr'
$releaseRoot = Join-Path $snapshotRoot 'release'
$languages = @('ENGLISH', 'FRENCH', 'ITALIAN', 'GERMAN', 'SPANISH', 'POLISH', 'CHINESE', 'RUSSIAN', 'JAPANESE')
$requiredRuntime = @(
    'Scripts/WhereaboutsAPI.pex'
    'Scripts/WhereaboutsNative.pex'
    'Scripts/WhereaboutsQuest.pex'
    'Scripts/WhereaboutsTrackedAlias.pex'
    'LICENSES/CommonLibSSE-NG-EXCEPTIONS.md'
    'LICENSES/CommonLibSSE-NG-GPL-3.0.txt'
    'LICENSES/SKSE-Menu-Framework-API.txt'
    'LICENSES/Whereabouts-Permissions.txt'
    'LICENSES/Third-Party-Notices.txt'
    'SKSE/Plugins/Whereabouts.dll'
    'SKSE/Plugins/Whereabouts.ini'
    'Whereabouts.esp'
) + @($languages | ForEach-Object { "Interface/Translations/Whereabouts_$_.txt" })
$requiredRuntime = @($requiredRuntime | Sort-Object)

function Get-RelativeFiles([string]$Root) {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    @(Get-ChildItem -LiteralPath $rootPath -Recurse -File | ForEach-Object {
        $_.FullName.Substring($rootPath.Length + 1).Replace('\', '/')
    } | Sort-Object)
}

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
        }
        finally { $sha256.Dispose() }
    }
    finally { $stream.Dispose() }
}

function Assert-ArchiveMatchesStage([string]$ArchivePath, [string]$StageRoot) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $entries = @($archive.Entries)
        $archiveFiles = @($entries | ForEach-Object { $_.FullName.Replace('\', '/') } | Sort-Object)
        $stageFiles = Get-RelativeFiles $StageRoot
        if ([string]::Join("`n", $archiveFiles) -ne [string]::Join("`n", $stageFiles)) {
            throw "Archive inventory does not match its staging tree: $([System.IO.Path]::GetFileName($ArchivePath))"
        }
        foreach ($entry in $entries) {
            $stagePath = Join-Path $StageRoot ($entry.FullName -replace '/', '\')
            $stream = $entry.Open()
            try {
                $sha256 = [System.Security.Cryptography.SHA256]::Create()
                try {
                    $archiveHash = ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
                }
                finally { $sha256.Dispose() }
            }
            finally { $stream.Dispose() }
            $stageHash = Get-Sha256 $stagePath
            if ($archiveHash -ne $stageHash) {
                throw "Archive payload differs from staging: $($entry.FullName)"
            }
        }
    }
    finally { $archive.Dispose() }
}

function Assert-RuntimeBinaryHygiene([string]$DllPath) {
    $binaryText = [System.Text.Encoding]::GetEncoding(28591).GetString([System.IO.File]::ReadAllBytes($DllPath))
    $absolutePaths = @([regex]::Matches(
        $binaryText,
        '(?i)(?<![a-z0-9+.-])[a-z]:[\\/](?![\\/])[\x20-\x7E]{3,240}') |
        ForEach-Object { $_.Value } | Sort-Object -Unique)
    if ($absolutePaths.Count -ne 0) {
        throw "Runtime DLL contains absolute machine paths: $($absolutePaths -join '; ')"
    }
    $markers = @(
        (-join @(67, 104, 97, 116, 71, 80, 84 | ForEach-Object { [char]$_ })),
        (-join @(79, 112, 101, 110, 65, 73 | ForEach-Object { [char]$_ })),
        (-join @(103, 101, 110, 101, 114, 97, 116, 101, 100, 32, 98, 121, 32, 65, 73 | ForEach-Object { [char]$_ }))
    )
    foreach ($marker in $markers) {
        if ($binaryText.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw 'Runtime DLL contains a disallowed generated-content marker.'
        }
    }
}

function Assert-PexMetadata([string]$Root) {
    $validator = Join-Path $snapshotRoot 'tools/normalize-pex-metadata.ps1'
    foreach ($pex in Get-ChildItem -LiteralPath (Join-Path $Root 'Scripts') -Filter '*.pex' -File) {
        & $validator -Path $pex.FullName -ValidateOnly
    }
}

function Assert-RuntimeStagesDifferOnlyByDll([string]$MainRoot, [string]$VrRoot) {
    $mainFiles = Get-RelativeFiles $MainRoot
    $vrFiles = Get-RelativeFiles $VrRoot
    if ([string]::Join("`n", $mainFiles) -ne [string]::Join("`n", $vrFiles)) {
        throw 'Main and VR staging inventories differ.'
    }
    foreach ($relative in $mainFiles) {
        $mainHash = Get-Sha256 (Join-Path $MainRoot ($relative -replace '/', '\'))
        $vrHash = Get-Sha256 (Join-Path $VrRoot ($relative -replace '/', '\'))
        if ($relative -eq 'SKSE/Plugins/Whereabouts.dll') {
            if ($mainHash -eq $vrHash) { throw 'Main and VR runtime DLLs must be distinct builds.' }
        }
        elseif ($mainHash -ne $vrHash) {
            throw "Main and VR shared payload differs: $relative"
        }
    }
}

if (-not (Test-Path -LiteralPath $runtimeStage -PathType Container)) { throw 'Runtime stage is missing.' }
$actualRuntime = Get-RelativeFiles $runtimeStage
if ([string]::Join("`n", $actualRuntime) -ne [string]::Join("`n", $requiredRuntime)) {
    throw 'Runtime stage does not match the explicit deployable allowlist.'
}
Assert-RuntimeBinaryHygiene (Join-Path $runtimeStage 'SKSE/Plugins/Whereabouts.dll')
Assert-PexMetadata $runtimeStage
if ($RequireVrArchive) {
    if (-not (Test-Path -LiteralPath $vrStage -PathType Container)) { throw 'VR stage is missing.' }
    $actualVr = Get-RelativeFiles $vrStage
    if ([string]::Join("`n", $actualVr) -ne [string]::Join("`n", $requiredRuntime)) {
        throw 'VR stage does not match the explicit deployable allowlist.'
    }
    Assert-RuntimeBinaryHygiene (Join-Path $vrStage 'SKSE/Plugins/Whereabouts.dll')
    Assert-PexMetadata $vrStage
    Assert-RuntimeStagesDifferOnlyByDll $runtimeStage $vrStage
}

if ($RequireArchives) {
    $archives = [ordered]@{
        "Whereabouts-$Version-Main.zip" = $runtimeStage
    }
    if ($RequireVrArchive) {
        $archives["Whereabouts-$Version-VR-Untested.zip"] = $vrStage
    }
    $expectedArchives = @($archives.Keys | Sort-Object)
    $actualArchives = @(Get-ChildItem -LiteralPath $releaseRoot -Filter '*.zip' -File |
        Select-Object -ExpandProperty Name | Sort-Object)
    if ([string]::Join("`n", $actualArchives) -ne [string]::Join("`n", $expectedArchives)) {
        throw 'Release directory does not contain exactly the required runtime ZIPs.'
    }
    foreach ($item in $archives.GetEnumerator()) {
        $archivePath = Join-Path $releaseRoot $item.Key
        if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
            throw "Archive is missing: $($item.Key)"
        }
        Assert-ArchiveMatchesStage $archivePath $item.Value
        Write-Output "$($item.Key) SHA256=$(Get-Sha256 $archivePath)"
    }
}

Write-Output 'RESULT=PASS'
