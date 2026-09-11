[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SkyrimExecutable,
    [Parameter(Mandatory = $true)][string]$SkseDll,
    [Parameter(Mandatory = $true)][string]$ExpectedSkyrimFileVersion,
    [Parameter(Mandatory = $true)][string]$ExpectedSkseFileVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-FileVersion(
    [string]$Path,
    [string]$Expected,
    [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing: $Path"
    }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($resolved)
    $actual = '{0}.{1}.{2}.{3}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart, $info.FilePrivatePart
    if ($actual -ne $Expected) {
        throw "$Label file version mismatch: expected $Expected, found $actual"
    }
    $stream = [System.IO.File]::OpenRead($resolved)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
    }
    finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
    Write-Output "$Label file version $actual; SHA-256 $hash"
}

Assert-FileVersion -Path $SkyrimExecutable -Expected $ExpectedSkyrimFileVersion -Label 'Skyrim executable'
Assert-FileVersion -Path $SkseDll -Expected $ExpectedSkseFileVersion -Label 'SKSE runtime DLL'
