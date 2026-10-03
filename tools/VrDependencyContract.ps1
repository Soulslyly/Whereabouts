Set-StrictMode -Version Latest

# Version/provider policy only: binary consumers must also validate all exports.
function Get-WhereaboutsMenuFrameworkVersionPolicy {
    param([string]$FileName, [AllowNull()][version]$Version,
          [bool]$AllowUntestedMenuFrameworks = $true)
    $provider = 'unknown menu framework provider'
    $recognized = $false
    $legacy = $false
    if ($FileName -ieq 'SKSEMenuFramework.dll') {
        $provider = 'SKSE Menu Framework'
        if ($null -ne $Version) {
            $recognized = $Version.Major -eq 3 -and $Version.Minor -ge 14
            $legacy = $Version.Major -lt 3 -or ($Version.Major -eq 3 -and $Version.Minor -lt 14)
        }
    } elseif ($FileName -imatch '^!?ApocryphaMenuFramework\.dll$') {
        $provider = 'ApocryphaRealm Menu Framework'
        if ($null -ne $Version) {
            $recognized = $Version.Major -eq 2 -or
                ($Version.Major -eq 1 -and $Version -ge [version]'1.8.4.0')
            $legacy = $Version.Major -lt 1 -or
                ($Version.Major -eq 1 -and $Version -lt [version]'1.8.4.0')
        }
    }
    if ($legacy) { throw "Known unsupported legacy $provider version: $Version" }
    if (-not $recognized -and -not $AllowUntestedMenuFrameworks) {
        throw "Untested menu framework rejected by strict checks: $FileName $Version"
    }
    [pscustomobject]@{ Provider = $provider; Untested = -not $recognized }
}

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
        [Parameter(Mandatory = $true)][AllowNull()][version]$MenuFrameworkVersion,
        [bool]$AllowUntestedMenuFrameworks = $true
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

    $policy = Get-WhereaboutsMenuFrameworkVersionPolicy -FileName $MenuFrameworkFileName `
        -Version $MenuFrameworkVersion -AllowUntestedMenuFrameworks $AllowUntestedMenuFrameworks

    [pscustomobject]@{
        MenuFrameworkProvider = $policy.Provider
        MenuFrameworkVersion = $MenuFrameworkVersion
        UntestedMenuFramework = $policy.Untested
    }
}
