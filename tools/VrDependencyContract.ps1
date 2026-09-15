Set-StrictMode -Version Latest

function Test-WhereaboutsVrDependencyValues {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$SkyrimFileName,
        [Parameter(Mandatory = $true)][string]$SkyrimVersion,
        [Parameter(Mandatory = $true)][string]$SkseFileName,
        [Parameter(Mandatory = $true)][string]$SkseVersion,
        [Parameter(Mandatory = $true)][string]$AddressLibraryFileName,
        [Parameter(Mandatory = $true)][string]$AddressLibraryVersion,
        [Parameter(Mandatory = $true)][string]$VrEslSupportFileName,
        [Parameter(Mandatory = $true)][string]$VrEslSupportVersion,
        [Parameter(Mandatory = $true)][string]$SmfFileName,
        [Parameter(Mandatory = $true)][string]$SmfVersion
    )

    if ($SkyrimFileName -ine 'SkyrimVR.exe' -or $SkyrimVersion -ne '1.4.15.0') {
        throw "Skyrim VR mismatch: $SkyrimFileName $SkyrimVersion"
    }
    if ($SkseFileName -ine 'sksevr_1_4_15.dll' -or $SkseVersion -ne '0.2.0.12') {
        throw "SKSEVR mismatch: $SkseFileName $SkseVersion"
    }
    if ($AddressLibraryFileName -ine 'version-1-4-15-0.csv' -or
        [version]$AddressLibraryVersion -lt [version]'0.109.0') {
        throw "VR Address Library mismatch: $AddressLibraryFileName $AddressLibraryVersion"
    }
    if ($VrEslSupportFileName -ine 'skyrimvresl.dll' -or
        [version]$VrEslSupportVersion -lt [version]'1.3.2.0') {
        throw "Skyrim VR ESL Support mismatch: $VrEslSupportFileName $VrEslSupportVersion"
    }
    $smf = [version]$SmfVersion
    if ($SmfFileName -ine 'SKSEMenuFramework.dll' -or $smf.Major -ne 3 -or $smf.Minor -lt 14) {
        throw "SKSE Menu Framework mismatch: $SmfFileName $SmfVersion"
    }

    [pscustomobject]@{
        SkyrimVersion = $SkyrimVersion
        SkseVersion = $SkseVersion
        AddressLibraryVersion = $AddressLibraryVersion
        VrEslSupportVersion = $VrEslSupportVersion
        SmfVersion = $SmfVersion
    }
}
