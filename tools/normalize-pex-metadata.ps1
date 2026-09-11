[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$Path,
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$canonicalText = 'Whereabouts'
$canonicalBytes = [System.Text.Encoding]::UTF8.GetBytes($canonicalText)

function Read-BigEndianUInt16([byte[]]$Bytes, [int]$Offset, [string]$Label) {
    if ($Offset -lt 0 -or $Offset + 2 -gt $Bytes.Length) {
        throw "PEX header is truncated before $Label."
    }
    return ([int]$Bytes[$Offset] -shl 8) -bor [int]$Bytes[$Offset + 1]
}

function Read-PexStringSpan([byte[]]$Bytes, [ref]$Offset, [string]$Label) {
    $length = Read-BigEndianUInt16 $Bytes $Offset.Value "$Label length"
    $dataOffset = $Offset.Value + 2
    if ($dataOffset + $length -gt $Bytes.Length) {
        throw "PEX header is truncated inside $Label."
    }
    $Offset.Value = $dataOffset + $length
    return [pscustomobject]@{ LengthOffset = $dataOffset - 2; DataOffset = $dataOffset; Length = $length }
}

function Test-BytesEqual([byte[]]$Bytes, [int]$Offset, [byte[]]$Expected) {
    if ($Offset + $Expected.Length -gt $Bytes.Length) { return $false }
    for ($index = 0; $index -lt $Expected.Length; $index++) {
        if ($Bytes[$Offset + $index] -ne $Expected[$index]) { return $false }
    }
    return $true
}

function Add-BigEndianUInt16([System.Collections.Generic.List[byte]]$Bytes, [int]$Value) {
    if ($Value -lt 0 -or $Value -gt [uint16]::MaxValue) { throw 'PEX metadata string is too long.' }
    $Bytes.Add([byte](($Value -shr 8) -band 0xFF))
    $Bytes.Add([byte]($Value -band 0xFF))
}

foreach ($item in $Path) {
    if (-not (Test-Path -LiteralPath $item -PathType Leaf)) { throw "PEX file is missing: $item" }
    $resolved = (Resolve-Path -LiteralPath $item).Path
    $bytes = [System.IO.File]::ReadAllBytes($resolved)
    if ($bytes.Length -lt 16 -or
        $bytes[0] -ne 0xFA -or $bytes[1] -ne 0x57 -or
        $bytes[2] -ne 0xC0 -or $bytes[3] -ne 0xDE) {
        throw "PEX magic header is invalid: $resolved"
    }

    $offset = 16
    $source = Read-PexStringSpan $bytes ([ref]$offset) 'source filename'
    $user = Read-PexStringSpan $bytes ([ref]$offset) 'user name'
    $machine = Read-PexStringSpan $bytes ([ref]$offset) 'computer name'
    $timestampCanonical = -not @($bytes[8..15] | Where-Object { $_ -ne 0 }).Count
    $userCanonical = $user.Length -eq $canonicalBytes.Length -and
        (Test-BytesEqual $bytes $user.DataOffset $canonicalBytes)
    $machineCanonical = $machine.Length -eq $canonicalBytes.Length -and
        (Test-BytesEqual $bytes $machine.DataOffset $canonicalBytes)

    if ($ValidateOnly) {
        if (-not $timestampCanonical -or -not $userCanonical -or -not $machineCanonical) {
            throw "PEX compiler metadata is not canonical: $resolved"
        }
        Write-Output "PASS: canonical PEX metadata: $resolved"
        continue
    }

    $normalized = [System.Collections.Generic.List[byte]]::new($bytes.Length)
    $normalized.AddRange([byte[]]$bytes[0..7])
    $normalized.AddRange([byte[]](0..7 | ForEach-Object { 0 }))
    $normalized.AddRange([byte[]]$bytes[$source.LengthOffset..($source.DataOffset + $source.Length - 1)])
    Add-BigEndianUInt16 $normalized $canonicalBytes.Length
    $normalized.AddRange($canonicalBytes)
    Add-BigEndianUInt16 $normalized $canonicalBytes.Length
    $normalized.AddRange($canonicalBytes)
    if ($offset -lt $bytes.Length) {
        $normalized.AddRange([byte[]]$bytes[$offset..($bytes.Length - 1)])
    }
    [System.IO.File]::WriteAllBytes($resolved, $normalized.ToArray())
    Write-Output "PASS: normalized PEX metadata: $resolved"
}
