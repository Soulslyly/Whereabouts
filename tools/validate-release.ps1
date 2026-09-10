[CmdletBinding()]
param(
    [string]$Version,
    [switch]$RequireArchives,
    [switch]$SourceOnly,
    [string]$MenuFrameworkDll
)

$ErrorActionPreference = "Stop"
$snapshotRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')

function Get-CanonicalVersion {
    $contractPath = Join-Path $snapshotRoot 'version.json'
    if (-not (Test-Path -LiteralPath $contractPath -PathType Leaf)) {
        throw "Canonical version contract is missing: $contractPath"
    }
    $contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
    if ([string]::IsNullOrWhiteSpace($contract.display)) {
        throw 'Canonical display version is empty.'
    }
    [string]$contract.display
}

$canonicalVersion = Get-CanonicalVersion
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $canonicalVersion
}

if ($Version -ne $canonicalVersion) {
    throw "Requested version '$Version' does not match canonical version '$canonicalVersion'."
}

if ($RequireArchives -and -not $SourceOnly) {
    if ([string]::IsNullOrWhiteSpace($MenuFrameworkDll)) {
        throw 'MenuFrameworkDll is required for complete archive validation.'
    }
    $smfValidator = Join-Path $snapshotRoot 'tools/validate-smf-binary.ps1'
    & $smfValidator -MenuFrameworkDll $MenuFrameworkDll -ProjectRoot $snapshotRoot
}
$runtimeStage = Join-Path $snapshotRoot "staging/runtime"
$sourceStage = Join-Path $snapshotRoot "staging/source"
$translationStage = Join-Path $snapshotRoot "staging/translations"
$releaseRoot = Join-Path $snapshotRoot "release"
$requiredRuntime = @(
    "Scripts/WhereaboutsAPI.pex",
    "Scripts/WhereaboutsNative.pex",
    "Scripts/WhereaboutsQuest.pex",
    "Scripts/WhereaboutsTrackedAlias.pex",
    "LICENSES/CommonLibSSE-NG-EXCEPTIONS.md",
    "LICENSES/CommonLibSSE-NG-GPL-3.0.txt",
    "LICENSES/SKSE-Menu-Framework-API.txt",
    "LICENSES/Whereabouts-Permissions.txt",
    "LICENSES/Third-Party-Notices.txt",
    "SKSE/Plugins/Whereabouts.dll",
    "SKSE/Plugins/Whereabouts.ini",
    "Whereabouts.esp"
) | Sort-Object
$languages = @('ENGLISH', 'FRENCH', 'ITALIAN', 'GERMAN', 'SPANISH', 'POLISH', 'CHINESE', 'RUSSIAN', 'JAPANESE')
$requiredRuntime += @($languages | ForEach-Object { "Interface/Translations/Whereabouts_$_.txt" })
$requiredRuntime = @($requiredRuntime | Sort-Object)
$requiredTranslations = @($languages | Where-Object { $_ -ne 'ENGLISH' } |
    ForEach-Object { "Interface/Translations/Whereabouts_$_.txt" } | Sort-Object)

function Get-RelativeFiles([string]$Root) {
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    @(Get-ChildItem -LiteralPath $rootPath -Recurse -File | ForEach-Object {
        $_.FullName.Substring($rootPath.Length + 1).Replace('\', '/')
    } | Sort-Object)
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
                finally {
                    $sha256.Dispose()
                }
            }
            finally {
                $stream.Dispose()
            }
            $stageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $stagePath).Hash
            if ($archiveHash -ne $stageHash) {
                throw "Archive payload differs from staging: $($entry.FullName)"
            }
        }
    }
    finally {
        $archive.Dispose()
    }
}

function Assert-RuntimeBinaryHygiene([string]$DllPath) {
    $binaryText = [System.Text.Encoding]::GetEncoding(28591).GetString([System.IO.File]::ReadAllBytes($DllPath))
    $absolutePaths = @(
        [regex]::Matches($binaryText, '(?i)(?<![a-z0-9+.-])[a-z]:[\\/](?![\\/])[\x20-\x7E]{3,240}') |
            ForEach-Object { $_.Value } |
            Sort-Object -Unique
    )
    if ($absolutePaths.Count -ne 0) {
        throw "Runtime DLL contains absolute machine paths: $($absolutePaths -join '; ')"
    }

    $disallowedAttributionMarkers = @(
        (-join @(67, 104, 97, 116, 71, 80, 84 | ForEach-Object { [char]$_ })),
        (-join @(79, 112, 101, 110, 65, 73 | ForEach-Object { [char]$_ })),
        (-join @(103, 101, 110, 101, 114, 97, 116, 101, 100, 32, 98, 121, 32, 65, 73 | ForEach-Object { [char]$_ }))
    )
    foreach ($marker in $disallowedAttributionMarkers) {
        if ($binaryText.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw "Runtime DLL contains a disallowed generated-content marker"
        }
    }
}

if (-not $SourceOnly -and -not (Test-Path -LiteralPath $runtimeStage -PathType Container)) { throw "Runtime stage is missing" }
if (-not $SourceOnly -and -not (Test-Path -LiteralPath $translationStage -PathType Container)) { throw "Translation stage is missing" }
if (-not (Test-Path -LiteralPath $sourceStage -PathType Container)) { throw "Source stage is missing" }
if (-not $SourceOnly) {
    $actualRuntime = Get-RelativeFiles $runtimeStage
    if ([string]::Join("`n", $actualRuntime) -ne [string]::Join("`n", $requiredRuntime)) {
        throw "Runtime stage does not match the explicit deployable allowlist"
    }
    $actualTranslations = Get-RelativeFiles $translationStage
    if ([string]::Join("`n", $actualTranslations) -ne [string]::Join("`n", $requiredTranslations)) {
        throw "Translation stage does not match the eight-file overwrite allowlist"
    }
    Assert-RuntimeBinaryHygiene (Join-Path $runtimeStage "SKSE/Plugins/Whereabouts.dll")
}

$sourceFiles = Get-RelativeFiles $sourceStage
$internalProjectMaterial = $sourceFiles | Where-Object {
    $_ -match '(^|/)(docs/internal|MEMORY)(/|$)' -or
    $_ -match '(^|/)\.[^/]+/' -or
    $_ -match '(^|/)(AGENTS|SKILL)\.md$'
}
if ($internalProjectMaterial) {
    throw "Source stage contains internal project material: $($internalProjectMaterial -join ', ')"
}

$denied = $sourceFiles | Where-Object {
    $_ -match '(^|/)(\.git|build|staging|release|bin|obj|\.deps|tests?)(/|$)' -or
    $_ -match '\.(dll|pex|esp|esm|esl|pdb|log|cache)$' -or
    $_ -match '(^|/)(PLAN|STATE|DECISIONS|VALIDATION|CURRENT|WORKSPACE_OWNERSHIP)\.(md|txt)$'
}
if ($denied) { throw "Source stage contains denied files: $($denied -join ', ')" }

$allowedProjectFiles = @(
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
    "tools/generate-translation-resources.ps1",
    "tools/stage-release.ps1",
    "tools/validate-release.ps1",
    "tools/validate-smf-binary.ps1",
    "tools/PluginBuilder/Whereabouts.PluginBuilder.csproj",
    "tools/PluginBuilder/Program.cs",
    "tools/PluginBuilder/PluginCommands.cs",
    "docs/DEPENDENCIES.md",
    "docs/COMPATIBILITY.md",
    "docs/TRANSLATING.md"
)
function Test-AllowedProjectSourcePath([string]$Path) {
    if ($Path -in $allowedProjectFiles) { return $true }
    if ($Path -match '^include/.+\.h$') { return $true }
    if ($Path -match '^src/.+\.(cpp|h)$') { return $true }
    if ($Path -eq 'src/UI/TranslationCatalog.inc') { return $true }
    if ($Path -match '^papyrus/Source/.+\.psc$') { return $true }
    if ($Path -match '^Interface/Translations/Whereabouts_(ENGLISH|FRENCH|ITALIAN|GERMAN|SPANISH|POLISH|CHINESE|RUSSIAN|JAPANESE)\.txt$') { return $true }
    if ($Path -match '^translations/machine/Whereabouts_(FRENCH|ITALIAN|GERMAN|SPANISH|POLISH|CHINESE|RUSSIAN|JAPANESE)\.txt$') { return $true }
    if ($Path -match '^external/CommonLibSSE-NG/') { return $true }
    return $false
}
$unexpectedProjectFiles = $sourceFiles | Where-Object {
    -not (Test-AllowedProjectSourcePath $_)
}
if ($unexpectedProjectFiles) {
    throw "Source stage contains an unexpected source file type or path: $($unexpectedProjectFiles -join ', ')"
}

$commonLibAllowedFiles = @(
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
$unexpectedCommonLibFiles = $sourceFiles | Where-Object {
    $_ -like "external/CommonLibSSE-NG/*" -and
    $_ -notin $commonLibAllowedFiles -and
    $_ -notmatch '^external/CommonLibSSE-NG/(include|licenses|res|src)/'
}
if ($unexpectedCommonLibFiles) {
    throw "Source stage contains unnecessary CommonLib files: $($unexpectedCommonLibFiles -join ', ')"
}
$unexpectedCommonLibTypes = $sourceFiles | Where-Object {
    if ($_ -notmatch '^external/CommonLibSSE-NG/(include|licenses|res|src)/') {
        return $false
    }
    if ($_ -eq 'external/CommonLibSSE-NG/licenses/LICENSE-MIT') {
        return $false
    }
    [System.IO.Path]::GetExtension($_).ToLowerInvariant() -notin @('.cpp', '.h', '.in', '.md', '.ps1')
}
if ($unexpectedCommonLibTypes) {
    throw "Source stage contains unexpected CommonLib file types: $($unexpectedCommonLibTypes -join ', ')"
}

$requiredSource = @(
    "README.md",
    "CMakeLists.txt",
    "version.json",
    "cmake/WhereaboutsVersion.h.in",
    "cmake/WhereaboutsVersion.rc.in",
    "global.json",
    "src/Plugin.cpp",
    "src/UI/Localization.cpp",
    "src/UI/Localization.h",
    "src/UI/TranslationCatalog.inc",
    "docs/TRANSLATING.md",
    "papyrus/Source/WhereaboutsAPI.psc",
    "papyrus/Source/WhereaboutsQuest.psc",
    "LICENSES/SKSE-Menu-Framework-API.txt",
    "LICENSES/Whereabouts-Permissions.txt",
    "LICENSES/Third-Party-Notices.txt",
    "external/CommonLibSSE-NG/COPYING",
    "external/CommonLibSSE-NG/EXCEPTIONS.md"
)
foreach ($required in $requiredSource) {
    if ($sourceFiles -notcontains $required) { throw "Source stage is missing $required" }
}

$textFiles = Get-ChildItem -LiteralPath $sourceStage -Recurse -File | Where-Object {
    $_.Extension -in @('.md', '.txt', '.cpp', '.h', '.psc', '.ps1', '.json', '.cs', '.ini')
}
$disallowedAttributionMarkers = @(
    (-join @(67, 104, 97, 116, 71, 80, 84 | ForEach-Object { [char]$_ })),
    (-join @(79, 112, 101, 110, 65, 73 | ForEach-Object { [char]$_ })),
    (-join @(103, 101, 110, 101, 114, 97, 116, 101, 100, 32, 98, 121, 32, 65, 73 | ForEach-Object { [char]$_ }))
)
$absolutePathPattern = '(?im)(?<![a-z0-9+.-])[a-z]:[\\/](?![\\/])'
foreach ($file in $textFiles) {
    $content = Get-Content -LiteralPath $file.FullName -Raw
    if ($null -eq $content) { $content = "" }
    $containsDisallowedAttribution = $false
    foreach ($marker in $disallowedAttributionMarkers) {
        if ($content.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            $containsDisallowedAttribution = $true
            break
        }
    }
    if ([regex]::IsMatch($content, $absolutePathPattern) -or $containsDisallowedAttribution) {
        throw "Source archive text scan failed: $($file.FullName)"
    }
}

if ((Get-Content -LiteralPath (Join-Path $sourceStage "VERSION.txt") -TotalCount 1).Trim() -ne "Whereabouts $Version") {
    throw "VERSION.txt does not match $Version"
}

if ($RequireArchives) {
    $archives = [ordered]@{}
    if (-not $SourceOnly) { $archives["Whereabouts-$Version-Main.zip"] = $runtimeStage }
    if (-not $SourceOnly) { $archives["Whereabouts-$Version-Translations.zip"] = $translationStage }
    $archives["Whereabouts-$Version-Source.zip"] = $sourceStage
    foreach ($item in $archives.GetEnumerator()) {
        $name = $item.Key
        $archive = Join-Path $releaseRoot $name
        if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Archive is missing: $name" }
        Assert-ArchiveMatchesStage $archive $item.Value
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
        Write-Output "$name SHA256=$hash"
    }
}

Write-Output "RESULT=PASS"
