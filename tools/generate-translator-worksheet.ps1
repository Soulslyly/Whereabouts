[CmdletBinding()]
param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))

$ErrorActionPreference = 'Stop'
$catalogPath = Join-Path $ProjectRoot 'src/UI/TranslationCatalog.inc'
$worksheetPath = Join-Path $ProjectRoot 'translations/Whereabouts_1.1.0_Translator_Worksheet.tsv'
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("Internal key`tEnglish source`tTranslation`tPlaceholders")
foreach ($line in Get-Content -LiteralPath $catalogPath -Encoding utf8) {
    if ($line -notmatch '^WA_TEXT\(([^,]+),\s*"([^"]*)"\)$') {
        throw "Invalid catalog row: $line"
    }
    $key = '$Whereabouts_' + $Matches[1]
    $english = $Matches[2]
    if ($english.Contains("`t") -or $english.Contains("`r") -or $english.Contains("`n")) {
        throw "Worksheet source contains unsupported whitespace: $key"
    }
    $placeholders = ([regex]::Matches($english, '\{[^{}]*\}') | ForEach-Object Value) -join ' '
    $lines.Add("$key`t$english`t`t$placeholders")
}
New-Item -ItemType Directory -Path (Split-Path -Parent $worksheetPath) -Force | Out-Null
[IO.File]::WriteAllText(
    $worksheetPath,
    (($lines -join "`r`n") + "`r`n"),
    [Text.UTF8Encoding]::new($true))
Write-Output "Generated translator worksheet with $($lines.Count - 1) catalog rows."
