[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][string]$SkyrimVrExecutable,
    [Parameter(Mandatory = $true)][string]$SkseVrDll,
    [Parameter(Mandatory = $true)][string]$VrAddressLibrary,
    [Parameter(Mandatory = $true)][string]$VrEslSupportDll,
    [Parameter(Mandatory = $true)][string]$MenuFrameworkDll
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'VrDependencyContract.ps1')

function Get-RequiredFile([string]$Path, [string]$Label) {
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolved.Path -PathType Leaf)) {
        throw "$Label must name an exact installed file: $Path"
    }
    Get-Item -LiteralPath $resolved.Path
}

function Get-NumericFileVersion([string]$Path) {
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    '{0}.{1}.{2}.{3}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart, $info.FilePrivatePart
}

$skyrim = Get-RequiredFile $SkyrimVrExecutable 'Skyrim VR executable'
$skse = Get-RequiredFile $SkseVrDll 'SKSEVR runtime DLL'
$addressLibrary = Get-RequiredFile $VrAddressLibrary 'VR Address Library database'
$vrEsl = Get-RequiredFile $VrEslSupportDll 'Skyrim VR ESL Support DLL'
$menuFramework = Get-RequiredFile $MenuFrameworkDll 'Menu framework DLL'

$addressRows = @(Import-Csv -LiteralPath $addressLibrary.FullName)
if ($addressRows.Count -lt 2 -or
    $addressRows[0].PSObject.Properties.Name -notcontains 'id' -or
    $addressRows[0].PSObject.Properties.Name -notcontains 'offset' -or
    -not ($addressRows[0].id -as [uint32])) {
    throw 'The VR Address Library CSV metadata is malformed.'
}
$declaredRows = [uint32]$addressRows[0].id
if ($addressRows.Count -ne ($declaredRows + 1)) {
    throw "VR Address Library row count mismatch: declared $declaredRows, found $($addressRows.Count - 1)."
}

$values = @{
    SkyrimFileName = $skyrim.Name
    SkyrimVersion = Get-NumericFileVersion $skyrim.FullName
    SkseFileName = $skse.Name
    SkseVersion = Get-NumericFileVersion $skse.FullName
    AddressLibraryFileName = $addressLibrary.Name
    AddressLibraryVersion = [string]$addressRows[0].offset
    VrEslSupportFileName = $vrEsl.Name
    VrEslSupportVersion = Get-NumericFileVersion $vrEsl.FullName
    MenuFrameworkFileName = $menuFramework.Name
    MenuFrameworkVersion = Get-NumericFileVersion $menuFramework.FullName
}
$result = Test-WhereaboutsVrDependencyValues @values

& (Join-Path $ProjectRoot 'tools/validate-smf-binary.ps1') -MenuFrameworkDll $menuFramework.FullName -ProjectRoot $ProjectRoot
if ($LASTEXITCODE -ne 0) {
    throw 'The menu-framework binary validation failed.'
}

Write-Output "Skyrim VR: $($values.SkyrimVersion)"
Write-Output "SKSEVR: $($values.SkseVersion)"
Write-Output "VR Address Library: $($values.AddressLibraryVersion) ($($addressRows.Count) rows)"
Write-Output "Skyrim VR ESL Support: $($values.VrEslSupportVersion)"
Write-Output "$($result.MenuFrameworkProvider): $($result.MenuFrameworkVersion)"
