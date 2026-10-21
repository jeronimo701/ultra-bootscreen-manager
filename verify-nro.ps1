$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'UltraBootscreenManager.nro'))
function U32([long]$offset) { [BitConverter]::ToUInt32($bytes, [int]$offset) }
function U64([long]$offset) { [BitConverter]::ToUInt64($bytes, [int]$offset) }
if ([Text.Encoding]::ASCII.GetString($bytes, 16, 4) -ne 'NRO0') { throw 'Invalid NRO header' }
$assetBase = U32 24
if ([Text.Encoding]::ASCII.GetString($bytes, $assetBase, 4) -ne 'ASET') { throw 'Missing asset header' }
$romBase = $assetBase + (U64 ($assetBase + 40))
$romSize = U64 ($assetBase + 48)
if ($romBase + $romSize -gt $bytes.Length) { throw 'RomFS exceeds NRO length' }
$metaStart = $romBase + (U64 ($romBase + 56))
$metaSize = U64 ($romBase + 64)
$dataStart = $romBase + (U64 ($romBase + 72))
$position = $metaStart
$found = @()
while ($position -lt $metaStart + $metaSize) {
    $offset = U64 ($position + 8)
    $size = U64 ($position + 16)
    $nameLen = U32 ($position + 28)
    if ($nameLen -gt 255 -or $position + 32 + $nameLen -gt $metaStart + $metaSize) { throw 'Invalid file metadata' }
    $name = [Text.Encoding]::UTF8.GetString($bytes, [int]($position + 32), [int]$nameLen)
    if ($name -notin @('15a.bmp', '17e.bmp', '04a.bmp', 'ultranx-original.bmp', 'font.bin')) { throw "Unexpected asset: $name" }
    if ($dataStart + $offset + $size -gt $romBase + $romSize) { throw "Asset out of bounds: $name" }
    $expected = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot "romfs\$name"))
    if ($expected.Length -ne $size) { throw "Wrong size: $name" }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $actualHash = [BitConverter]::ToString($sha.ComputeHash($bytes, [int]($dataStart + $offset), [int]$size))
        $expectedHash = [BitConverter]::ToString($sha.ComputeHash($expected))
    } finally { $sha.Dispose() }
    if ($actualHash -ne $expectedHash) { throw "Asset mismatch: $name" }
    $found += $name
    [pscustomobject]@{EmbeddedAsset=$name; Bytes=$size; SHA256=$actualHash.Replace('-', '')}
    $position += 32 + (($nameLen + 3) -band (-bnot 3))
}
if ($found.Count -ne 5 -or ($found | Sort-Object -Unique).Count -ne 5) { throw 'Expected exactly five unique assets' }
Write-Output "PASS: NRO header, RomFS bounds, four BMPs and Cyrillic font hashes."
