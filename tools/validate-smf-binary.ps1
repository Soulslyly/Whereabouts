[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$MenuFrameworkDll,

    [Parameter(Mandatory)]
    [string]$ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Read-UInt16([byte[]]$Bytes, [int]$Offset) {
    [BitConverter]::ToUInt16($Bytes, $Offset)
}

function Read-UInt32([byte[]]$Bytes, [int]$Offset) {
    [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Read-AsciiZ([byte[]]$Bytes, [int]$Offset) {
    $end = $Offset
    while ($end -lt $Bytes.Length -and $Bytes[$end] -ne 0) { $end++ }
    [Text.Encoding]::ASCII.GetString($Bytes, $Offset, $end - $Offset)
}

function Get-PeExports([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
    if ($bytes.Length -lt 0x100 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
        throw 'SMF DLL is not a valid PE image.'
    }
    $peOffset = [int](Read-UInt32 $bytes 0x3C)
    if ((Read-UInt32 $bytes $peOffset) -ne 0x00004550) { throw 'SMF PE signature is missing.' }
    $sectionCount = [int](Read-UInt16 $bytes ($peOffset + 6))
    $optionalSize = [int](Read-UInt16 $bytes ($peOffset + 20))
    $optionalOffset = $peOffset + 24
    if ((Read-UInt16 $bytes $optionalOffset) -ne 0x20B) { throw 'Expected an x64 SMF DLL.' }

    $sections = @()
    $sectionOffset = $optionalOffset + $optionalSize
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $offset = $sectionOffset + ($i * 40)
        $sections += [pscustomobject]@{
            VirtualSize = [uint32](Read-UInt32 $bytes ($offset + 8))
            VirtualAddress = [uint32](Read-UInt32 $bytes ($offset + 12))
            RawSize = [uint32](Read-UInt32 $bytes ($offset + 16))
            RawOffset = [uint32](Read-UInt32 $bytes ($offset + 20))
        }
    }

    function Convert-Rva([uint32]$Rva) {
        foreach ($section in $sections) {
            $span = [Math]::Max([uint64]$section.VirtualSize, [uint64]$section.RawSize)
            if ([uint64]$Rva -ge $section.VirtualAddress -and
                [uint64]$Rva -lt ([uint64]$section.VirtualAddress + $span)) {
                return [int]([uint64]$section.RawOffset + ([uint64]$Rva - $section.VirtualAddress))
            }
        }
        throw ('SMF export RVA 0x{0:X8} is outside the PE sections.' -f $Rva)
    }

    $exportRva = [uint32](Read-UInt32 $bytes ($optionalOffset + 112))
    if ($exportRva -eq 0) { throw 'SMF DLL has no export directory.' }
    $exportOffset = Convert-Rva $exportRva
    $nameCount = [int](Read-UInt32 $bytes ($exportOffset + 24))
    $namesOffset = Convert-Rva ([uint32](Read-UInt32 $bytes ($exportOffset + 32)))
    $exports = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for ($i = 0; $i -lt $nameCount; $i++) {
        $nameRva = [uint32](Read-UInt32 $bytes ($namesOffset + ($i * 4)))
        [void]$exports.Add((Read-AsciiZ $bytes (Convert-Rva $nameRva)))
    }
    $exports
}

$dllPath = (Resolve-Path -LiteralPath $MenuFrameworkDll).Path
$version = (Get-Item -LiteralPath $dllPath).VersionInfo
if ($version.FileMajorPart -ne 3 -or $version.FileMinorPart -lt 14) {
    throw "Unsupported installed SMF fixed version: $($version.FileVersion)"
}

$exports = Get-PeExports $dllPath
$required = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($name in @('RegisterEventPriority', 'UnregisterEvent', 'AddSectionItem', 'GetMainWindow')) {
    [void]$required.Add($name)
}

$header = Get-Content -LiteralPath (Join-Path $ProjectRoot 'include\SKSEMenuFramework.h') -Raw
$sourceText = (@(
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'src') -Recurse -File -Filter '*.cpp' |
        ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }
) -join "`n")
$usedWrappers = [regex]::Matches($sourceText, 'ImGuiMCP::([A-Za-z][A-Za-z0-9_]*)\s*\(') |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique

foreach ($wrapper in $usedWrappers) {
    if ($wrapper -in @('ImVec2', 'ImVec4')) { continue }
    $pattern = '(?s)inline\s+[^\{;]*?\b' + [regex]::Escape($wrapper) +
        '\s*\([^;\{]*\)\s*\{(?<body>.*?)(?=\r?\n\s*inline\s)'
    $definitions = [regex]::Matches($header, $pattern)
    if ($definitions.Count -eq 0) { throw "Used SMF wrapper has no inline definition: $wrapper" }
    $foundExport = $false
    foreach ($definition in $definitions) {
        foreach ($match in [regex]::Matches(
                $definition.Groups['body'].Value,
                'GetMenuFrameworkFunction<[^;]+?>\("([^"]+)"\)')) {
            [void]$required.Add($match.Groups[1].Value)
            $foundExport = $true
        }
    }
    if (-not $foundExport) { throw "Used SMF wrapper exposes no imported function: $wrapper" }
}

$missing = @($required | Where-Object { -not $exports.Contains($_) } | Sort-Object)
if ($missing.Count -ne 0) {
    throw "Installed SMF DLL is missing required exports: $($missing -join ', ')"
}

$runtimeProbe = Get-Content -LiteralPath (
    Join-Path $ProjectRoot 'src\Compatibility\MenuFrameworkRuntime.cpp') -Raw
$runtimeMissing = @($required | Where-Object {
    $runtimeProbe.IndexOf(('"{0}"' -f $_), [StringComparison]::Ordinal) -lt 0
} | Sort-Object)
if ($runtimeMissing.Count -ne 0) {
    throw "Runtime SMF preflight is missing used exports: $($runtimeMissing -join ', ')"
}

$stream = [IO.File]::OpenRead($dllPath)
try {
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try { $hash = ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '') }
    finally { $sha256.Dispose() }
}
finally { $stream.Dispose() }
Write-Output (
    'PASS: installed SMF fixed version {0}; {1} required exports; SHA-256 {2}' -f
    $version.FileVersion,
    $required.Count,
    $hash)
