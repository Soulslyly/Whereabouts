[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$CompilerPath,

    [string]$FlagsPath,

    [string[]]$ImportPath = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $projectRoot 'papyrus\Source'
$outputRoot = if ($Configuration -eq 'Release') {
    Join-Path $projectRoot 'papyrus\Compiled'
} else {
    Join-Path $projectRoot ("papyrus\bin\{0}" -f $Configuration)
}
$scriptNames = @('WhereaboutsAPI', 'WhereaboutsNative', 'WhereaboutsQuest', 'WhereaboutsTrackedAlias')

function Resolve-ExistingFile {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

$compilerCandidates = @($CompilerPath, $env:PAPYRUS_COMPILER)
$compilerCommand = Get-Command 'PapyrusCompiler.exe' -ErrorAction SilentlyContinue
if ($compilerCommand) {
    $compilerCandidates += $compilerCommand.Source
}
$resolvedCompiler = Resolve-ExistingFile $compilerCandidates
if (-not $resolvedCompiler) {
    Write-Output 'UNBUILT: PapyrusCompiler.exe was not found. Pass -CompilerPath or set PAPYRUS_COMPILER.'
    exit 3
}

$flagsCandidates = @($FlagsPath, $env:PAPYRUS_FLAGS)
$compilerDirectory = Split-Path -Parent $resolvedCompiler
$flagsCandidates += (Join-Path $compilerDirectory 'TESV_Papyrus_Flags.flg')
$resolvedFlags = Resolve-ExistingFile $flagsCandidates
if (-not $resolvedFlags) {
    Write-Output 'UNBUILT: TESV_Papyrus_Flags.flg was not found. Pass -FlagsPath or set PAPYRUS_FLAGS.'
    exit 3
}

$imports = [System.Collections.Generic.List[string]]::new()
$imports.Add($sourceRoot)
foreach ($path in $ImportPath) {
    if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path -LiteralPath $path -PathType Container)) {
        $imports.Add((Resolve-Path -LiteralPath $path).Path)
    }
}
if (-not [string]::IsNullOrWhiteSpace($env:PAPYRUS_IMPORTS)) {
    foreach ($path in ($env:PAPYRUS_IMPORTS -split ';')) {
        if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path -LiteralPath $path -PathType Container)) {
            $imports.Add((Resolve-Path -LiteralPath $path).Path)
        }
    }
}
$imports = @($imports | Select-Object -Unique)

$requiredParents = @('Actor.psc', 'Debug.psc', 'Form.psc', 'Game.psc', 'ObjectReference.psc', 'Quest.psc', 'ReferenceAlias.psc')
$missingParents = @(foreach ($parent in $requiredParents) {
    if (-not ($imports | Where-Object { Test-Path -LiteralPath (Join-Path $_ $parent) -PathType Leaf })) {
        $parent
    }
})
if ($missingParents.Count -gt 0) {
    Write-Output ("UNBUILT: missing Papyrus parent sources: {0}. Pass -ImportPath or set PAPYRUS_IMPORTS." -f ($missingParents -join ', '))
    exit 3
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
foreach ($scriptName in $scriptNames) {
    $oldOutput = Join-Path $outputRoot ($scriptName + '.pex')
    if (Test-Path -LiteralPath $oldOutput) {
        Remove-Item -LiteralPath $oldOutput -Force
    }
}

$importArgument = $imports -join ';'
foreach ($scriptName in $scriptNames) {
    $sourcePath = Join-Path $sourceRoot ($scriptName + '.psc')
    & $resolvedCompiler $sourcePath ("-f={0}" -f $resolvedFlags) ("-i={0}" -f $importArgument) ("-o={0}" -f $outputRoot)
    if ($LASTEXITCODE -ne 0) {
        throw "PapyrusCompiler failed for $scriptName with exit code $LASTEXITCODE."
    }
}

$outputs = foreach ($scriptName in $scriptNames) {
    $path = Join-Path $outputRoot ($scriptName + '.pex')
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "PapyrusCompiler did not create $path."
    }
    $file = Get-Item -LiteralPath $path
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($path)
    try {
        $hash = ($algorithm.ComputeHash($stream) | ForEach-Object { $_.ToString('X2') }) -join ''
    }
    finally {
        $stream.Dispose()
        $algorithm.Dispose()
    }
    [pscustomobject]@{
        file = $file.Name
        bytes = $file.Length
        sha256 = $hash
    }
}

$manifestPath = Join-Path $outputRoot 'papyrus-build-manifest.json'
[pscustomobject]@{
    configuration = $Configuration
    compiler = $resolvedCompiler
    flags = $resolvedFlags
    imports = $imports
    outputs = $outputs
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8

Write-Output "PASS: compiled four fresh PEX files to $outputRoot"
