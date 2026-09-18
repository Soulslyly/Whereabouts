Set-StrictMode -Version Latest

function Test-WhereaboutsVrDependencyValues {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$SkyrimFileName,
        [Parameter(Mandatory = $true)][version]$SkyrimVersion,
        [Parameter(Mandatory = $true)][string]$SkseFileName,
        [Parameter(Mandatory = $true)][version]$SkseVersion,
        [Parameter(Mandatory = $true)][string]$AddressLibraryFileName,
        [Parameter(Mandatory = $true)][version]$AddressLibraryVersion,
        [Parameter(Mandatory = $true)][string]$VrEslSupportFileName,
        [Parameter(Mandatory = $true)][version]$VrEslSupportVersion,
        [Parameter(Mandatory = $true)][string]$MenuFrameworkFileName,
        [Parameter(Mandatory = $true)][version]$MenuFrameworkVersion
    )

    if ($SkyrimFileName -ne 'SkyrimVR.exe' -or $SkyrimVersion -ne [version]'1.4.15.0') {
        throw 'Whereabouts VR requires SkyrimVR.exe 1.4.15.0.'
    }
    if ($SkseFileName -ne 'sksevr_1_4_15.dll' -or $SkseVersion -ne [version]'0.2.0.12') {
        throw 'Whereabouts VR requires SKSEVR 2.0.12 for Skyrim VR 1.4.15.'
    }
    if ($AddressLibraryFileName -ne 'version-1-4-15-0.csv' -or
        $AddressLibraryVersion -lt [version]'0.109.0') {
        throw 'Whereabouts VR requires Address Library for SKSEVR 0.109 or newer for runtime 1.4.15.'
    }
    if ($VrEslSupportFileName -ne 'skyrimvresl.dll' -or
        $VrEslSupportVersion -lt [version]'1.3.2.0') {
        throw 'Whereabouts VR requires Skyrim VR ESL Support 1.3.2 or newer.'
    }

    $provider = switch -Regex ($MenuFrameworkFileName) {
        '^SKSEMenuFramework\.dll$' {
            if ($MenuFrameworkVersion.Major -ne 3 -or $MenuFrameworkVersion.Minor -lt 14) {
                throw 'Whereabouts requires SKSE Menu Framework 3.14.x or newer within major version 3.'
            }
            'SKSE Menu Framework'
            break
        }
        '^!?ApocryphaMenuFramework\.dll$' {
            if ($MenuFrameworkVersion.Major -ne 1 -or $MenuFrameworkVersion -lt [version]'1.8.4.0') {
                throw 'Whereabouts requires ApocryphaRealm Menu Framework 1.8.4 or newer within major version 1.'
            }
            'ApocryphaRealm Menu Framework'
            break
        }
        default { throw 'The selected VR menu framework is not an accepted SMF or AMF provider.' }
    }

    [pscustomobject]@{
        MenuFrameworkProvider = $provider
        MenuFrameworkVersion = $MenuFrameworkVersion
    }
}
