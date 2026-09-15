[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][string]$SkyrimVrExecutable,
    [Parameter(Mandatory = $true)][string]$SkseVrDll,
    [Parameter(Mandatory = $true)][string]$VrAddressLibrary,
    [Parameter(Mandatory = $true)][string]$VrEslSupportDll,
    [Parameter(Mandatory = $true)][string]$SmfDll
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'VrDependencyContract.ps1')

function Resolve-RequiredFile([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "$Label is missing: $Path" }
    (Resolve-Path -LiteralPath $Path).Path
}

function Get-NumericFileVersion([string]$Path) {
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    '{0}.{1}.{2}.{3}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart, $info.FilePrivatePart
}

function Get-Sha256([string]$Path) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

$skyrim = Resolve-RequiredFile $SkyrimVrExecutable 'Skyrim VR executable'
$skse = Resolve-RequiredFile $SkseVrDll 'SKSEVR runtime DLL'
$address = Resolve-RequiredFile $VrAddressLibrary 'VR Address Library database'
$vresl = Resolve-RequiredFile $VrEslSupportDll 'Skyrim VR ESL Support DLL'
$smf = Resolve-RequiredFile $SmfDll 'SKSE Menu Framework DLL'

$rows = @(Import-Csv -LiteralPath $address)
if ($rows.Count -lt 2 -or
    $rows[0].PSObject.Properties.Name -notcontains 'id' -or
    $rows[0].PSObject.Properties.Name -notcontains 'offset' -or
    -not ($rows[0].id -as [uint32])) {
    throw 'VR Address Library CSV metadata is malformed.'
}
$declaredRows = [uint32]$rows[0].id
if ($rows.Count -ne ($declaredRows + 1)) {
    throw "VR Address Library row count mismatch: declared $declaredRows, found $($rows.Count - 1)."
}

$values = Test-WhereaboutsVrDependencyValues `
    -SkyrimFileName ([IO.Path]::GetFileName($skyrim)) `
    -SkyrimVersion (Get-NumericFileVersion $skyrim) `
    -SkseFileName ([IO.Path]::GetFileName($skse)) `
    -SkseVersion (Get-NumericFileVersion $skse) `
    -AddressLibraryFileName ([IO.Path]::GetFileName($address)) `
    -AddressLibraryVersion ([string]$rows[0].offset) `
    -VrEslSupportFileName ([IO.Path]::GetFileName($vresl)) `
    -VrEslSupportVersion (Get-NumericFileVersion $vresl) `
    -SmfFileName ([IO.Path]::GetFileName($smf)) `
    -SmfVersion (Get-NumericFileVersion $smf)

& (Join-Path $ProjectRoot 'tools/validate-smf-binary.ps1') `
    -MenuFrameworkDll $smf -ProjectRoot $ProjectRoot

Write-Output "Skyrim VR $($values.SkyrimVersion); SHA-256 $(Get-Sha256 $skyrim)"
Write-Output "SKSEVR $($values.SkseVersion); SHA-256 $(Get-Sha256 $skse)"
Write-Output "VR Address Library $($values.AddressLibraryVersion); SHA-256 $(Get-Sha256 $address)"
Write-Output "Skyrim VR ESL Support $($values.VrEslSupportVersion); SHA-256 $(Get-Sha256 $vresl)"
Write-Output "SKSE Menu Framework $($values.SmfVersion); SHA-256 $(Get-Sha256 $smf)"
