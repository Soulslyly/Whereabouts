[CmdletBinding()]
param(
    [string]$Version,
    [switch]$CreateArchives,
    [switch]$SourceOnly,
    [string]$RuntimeDllPath
)

$ErrorActionPreference = "Stop"
$snapshotRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
. (Join-Path $PSScriptRoot 'VersionContract.ps1')
$canonicalVersion = (Get-WhereaboutsVersionContract -ProjectRoot $snapshotRoot).display
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $canonicalVersion
}
elseif ($Version -ne $canonicalVersion) {
    throw "Requested version '$Version' does not match canonical version '$canonicalVersion'."
}
$stagingRoot = Join-Path $snapshotRoot "staging"
$runtimeStage = Join-Path $stagingRoot "runtime"
$sourceStage = Join-Path $stagingRoot "source"
$translationStage = Join-Path $stagingRoot "translations"
$releaseRoot = Join-Path $snapshotRoot "release"

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
    } else {
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
                $entry.LastWriteTime = [System.DateTimeOffset]::new(2020, 1, 1, 0, 0, 0, [System.TimeSpan]::Zero)
                $input = [System.IO.File]::OpenRead((Join-Path $SourceRoot ($relative -replace '/', '\')))
                try {
                    $output = $entry.Open()
                    try { $input.CopyTo($output) } finally { $output.Dispose() }
                } finally { $input.Dispose() }
            }
        } finally { $archive.Dispose() }
    } finally { $fileStream.Dispose() }
}

$runtimeDllSource = if ([string]::IsNullOrWhiteSpace($RuntimeDllPath)) {
    'build/vs2022-release/Release/Whereabouts.dll'
} else {
    [System.IO.Path]::GetFullPath($RuntimeDllPath)
}
$runtimeFiles = [ordered]@{
    "plugin/Whereabouts.esp" = "Whereabouts.esp"
    $runtimeDllSource = "SKSE/Plugins/Whereabouts.dll"
    "config/Whereabouts.ini" = "SKSE/Plugins/Whereabouts.ini"
    "papyrus/Compiled/WhereaboutsAPI.pex" = "Scripts/WhereaboutsAPI.pex"
    "papyrus/Compiled/WhereaboutsQuest.pex" = "Scripts/WhereaboutsQuest.pex"
    "papyrus/Compiled/WhereaboutsTrackedAlias.pex" = "Scripts/WhereaboutsTrackedAlias.pex"
    "papyrus/Compiled/WhereaboutsNative.pex" = "Scripts/WhereaboutsNative.pex"
    "external/CommonLibSSE-NG/COPYING" = "LICENSES/CommonLibSSE-NG-GPL-3.0.txt"
    "external/CommonLibSSE-NG/EXCEPTIONS.md" = "LICENSES/CommonLibSSE-NG-EXCEPTIONS.md"
    "LICENSES/SKSE-Menu-Framework-API.txt" = "LICENSES/SKSE-Menu-Framework-API.txt"
    "LICENSES/Whereabouts-Permissions.txt" = "LICENSES/Whereabouts-Permissions.txt"
    "LICENSES/Third-Party-Notices.txt" = "LICENSES/Third-Party-Notices.txt"
}
$languages = @('ENGLISH', 'FRENCH', 'ITALIAN', 'GERMAN', 'SPANISH', 'POLISH', 'CHINESE', 'RUSSIAN', 'JAPANESE')
foreach ($language in $languages) {
    $relative = "Interface/Translations/Whereabouts_$language.txt"
    $runtimeFiles[$relative] = $relative
}
if (-not $SourceOnly) {
    foreach ($source in $runtimeFiles.Keys) {
        $sourcePath = if ([System.IO.Path]::IsPathRooted($source)) {
            [System.IO.Path]::GetFullPath($source)
        } else {
            Join-Path $snapshotRoot $source
        }
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Required release file is missing: $source"
        }
    }
    Reset-SafeStage $runtimeStage
    foreach ($entry in $runtimeFiles.GetEnumerator()) {
        Copy-ReleaseFile $entry.Key $runtimeStage $entry.Value
    }
}

$sourceFiles = @(
    "CMakeLists.txt",
    "CMakePresets.json",
    "global.json",
    "vcpkg.json",
    "version.json",
    "VERSION.txt",
    "README.md",
    "CHANGELOG.md",
    "cmake/Dependencies.cmake",
    "cmake/OwnerValidation.cmake",
    "cmake/Packaging.cmake",
    "cmake/WhereaboutsVersion.h.in",
    "cmake/WhereaboutsVersion.rc.in",
    "cmake/triplets/x64-windows-static-md.cmake",
    "LICENSES/SKSE-Menu-Framework-API.txt",
    "LICENSES/Whereabouts-Permissions.txt",
    "LICENSES/Third-Party-Notices.txt",
    "config/Whereabouts.ini",
    "tools/compile-papyrus.ps1",
    "tools/normalize-pex-metadata.ps1",
    "tools/generate-translation-resources.ps1",
    "tools/stage-release.ps1",
    "tools/VersionContract.ps1",
    "tools/validate-release.ps1",
    "tools/validate-runtime-dependency.ps1",
    "tools/validate-smf-binary.ps1",
    "tools/PluginBuilder/Whereabouts.PluginBuilder.csproj",
    "tools/PluginBuilder/Program.cs",
    "tools/PluginBuilder/PluginCommands.cs",
    "docs/DEPENDENCIES.md",
    "docs/COMPATIBILITY.md",
    "docs/TRANSLATING.md"
)
foreach ($source in $sourceFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $snapshotRoot $source) -PathType Leaf)) {
        throw "Required source file is missing: $source"
    }
}
Reset-SafeStage $sourceStage
foreach ($file in $sourceFiles) {
    Copy-ReleaseFile $file $sourceStage $file
}
foreach ($language in $languages) {
    $relative = "Interface/Translations/Whereabouts_$language.txt"
    Copy-ReleaseFile $relative $sourceStage $relative
}
foreach ($language in $languages | Where-Object { $_ -ne 'ENGLISH' }) {
    $relative = "translations/machine/Whereabouts_$language.txt"
    Copy-ReleaseFile $relative $sourceStage $relative
}
foreach ($directory in @("include", "src", "papyrus/Source")) {
    $sourceDirectory = Join-Path $snapshotRoot ($directory -replace '/', '\')
    foreach ($file in Get-ChildItem -LiteralPath $sourceDirectory -Recurse -File) {
        $relative = $file.FullName.Substring($snapshotRoot.Length + 1).Replace('\', '/')
        Copy-ReleaseFile $relative $sourceStage $relative
    }
}
$commonLibFiles = @(
    "external/CommonLibSSE-NG/.clang-format",
    "external/CommonLibSSE-NG/CMakeLists.txt",
    "external/CommonLibSSE-NG/CommonLibSSE.natvis",
    "external/CommonLibSSE-NG/COPYING",
    "external/CommonLibSSE-NG/EXCEPTIONS.md",
    "external/CommonLibSSE-NG/UPSTREAM.txt",
    "external/CommonLibSSE-NG/cmake/CommonLibSSE.cmake",
    "external/CommonLibSSE-NG/cmake/Prebuilt.cmake",
    "external/CommonLibSSE-NG/cmake/config.cmake.in",
    "external/CommonLibSSE-NG/cmake/sourcelist.cmake",
    "external/CommonLibSSE-NG/cmake/testlist.cmake"
)
foreach ($file in $commonLibFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $snapshotRoot $file) -PathType Leaf)) {
        throw "Required CommonLib source file is missing: $file"
    }
    Copy-ReleaseFile $file $sourceStage $file
}
foreach ($directory in @(
    "external/CommonLibSSE-NG/include",
    "external/CommonLibSSE-NG/licenses",
    "external/CommonLibSSE-NG/res",
    "external/CommonLibSSE-NG/src"
)) {
    $sourceDirectory = Join-Path $snapshotRoot ($directory -replace '/', '\')
    foreach ($file in Get-ChildItem -LiteralPath $sourceDirectory -Recurse -File) {
        $relative = $file.FullName.Substring($snapshotRoot.Length + 1).Replace('\', '/')
        Copy-ReleaseFile $relative $sourceStage $relative
    }
}

if (-not $SourceOnly) {
    Reset-SafeStage $translationStage
    foreach ($language in $languages | Where-Object { $_ -ne 'ENGLISH' }) {
        $source = "translations/machine/Whereabouts_$language.txt"
        $destination = "Interface/Translations/Whereabouts_$language.txt"
        Copy-ReleaseFile $source $translationStage $destination
    }
}

if ($CreateArchives) {
    New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
    if (-not $SourceOnly) {
        New-DeterministicZip $runtimeStage (Join-Path $releaseRoot "Whereabouts-$Version-Main.zip")
        New-DeterministicZip $translationStage (Join-Path $releaseRoot "Whereabouts-$Version-Translations.zip")
    }
    New-DeterministicZip $sourceStage (Join-Path $releaseRoot "Whereabouts-$Version-Source.zip")
}

if (-not $SourceOnly) { Write-Output "RUNTIME_STAGE=$runtimeStage" }
if (-not $SourceOnly) { Write-Output "TRANSLATION_STAGE=$translationStage" }
Write-Output "SOURCE_STAGE=$sourceStage"
