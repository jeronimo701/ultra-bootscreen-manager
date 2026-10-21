$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$width = 16
$height = 26
$data = New-Object byte[] (384 * $width * $height)
$bitmap = New-Object System.Drawing.Bitmap($width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb))
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$font = New-Object System.Drawing.Font('Consolas', 22, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel))
try {
    for ($i=0; $i -lt 384; $i++) {
        $code = if($i -lt 128) { $i } else { 0x400 + $i - 128 }
        $graphics.Clear([System.Drawing.Color]::Transparent)
        if ($code -ge 32) {
            $graphics.DrawString(([char]$code).ToString(), $font, [System.Drawing.Brushes]::White, 0, 0, [System.Drawing.StringFormat]::GenericTypographic)
        }
        for ($y=0; $y -lt $height; $y++) {
            for ($x=0; $x -lt $width; $x++) {
                $data[($i*$width*$height)+($y*$width)+$x]=$bitmap.GetPixel($x,$y).A
            }
        }
    }
    [System.IO.File]::WriteAllBytes((Join-Path $PSScriptRoot 'romfs\font.bin'), $data)
} finally {
    $font.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
