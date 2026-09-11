Set-StrictMode -Version Latest

function Get-WhereaboutsVersionContract {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot
    )

    $contractPath = Join-Path $ProjectRoot 'version.json'
    if (-not (Test-Path -LiteralPath $contractPath -PathType Leaf)) {
        throw "Canonical version contract is missing: $contractPath"
    }
    $contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
    foreach ($property in @('numeric', 'display', 'parent')) {
        if ($contract.PSObject.Properties.Name -notcontains $property -or
            [string]::IsNullOrWhiteSpace([string]$contract.$property)) {
            throw "Canonical version contract is missing $property."
        }
    }

    $core = '(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)'
    $prereleaseIdentifier = '(?:0|[1-9]\d*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)'
    $buildIdentifier = '[0-9A-Za-z-]+'
    $semanticVersion = "$core(?:-$prereleaseIdentifier(?:\.$prereleaseIdentifier)*)?(?:\+$buildIdentifier(?:\.$buildIdentifier)*)?"

    $numeric = [string]$contract.numeric
    $display = [string]$contract.display
    $parent = [string]$contract.parent
    if ($numeric -notmatch "^$core$") {
        throw "Canonical numeric version is malformed: $numeric"
    }
    $displayPattern = '^' + [regex]::Escape($numeric) +
        "(?:-$prereleaseIdentifier(?:\.$prereleaseIdentifier)*)?(?:\+$buildIdentifier(?:\.$buildIdentifier)*)?$"
    if ($display -notmatch $displayPattern) {
        throw "Canonical display version is malformed or does not match numeric: $display"
    }
    if ($parent -notmatch "^$semanticVersion$" -or $parent -eq $display) {
        throw "Canonical parent version is malformed or not distinct: $parent"
    }

    [pscustomobject]@{
        numeric = $numeric
        display = $display
        parent = $parent
    }
}

