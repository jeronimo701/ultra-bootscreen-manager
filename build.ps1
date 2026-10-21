param([string]$Toolchain = $(if ($env:DEVKITPRO) { $env:DEVKITPRO } else { 'C:\devkitPro' }))
$ErrorActionPreference = 'Stop'
$previousDevkit = $env:DEVKITPRO
$toolRoot = [System.IO.Path]::GetFullPath($Toolchain)
$gcc = Join-Path $toolRoot 'devkitA64\bin\aarch64-none-elf-gcc.exe'
if (-not (Test-Path -LiteralPath $gcc)) { throw "Compiler missing: $gcc" }
$env:DEVKITPRO = $toolRoot.Replace('\', '/')
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path 'build' -Force | Out-Null
    & (Join-Path $PSScriptRoot 'make-font.ps1')
    $arch = @('-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-mtp=soft', '-fPIE')
    & $gcc '-O2' '-Wall' '-Wextra' '-Werror' '-std=gnu11' @arch "-I$toolRoot/libnx/include" '-c' 'source/app.c' '-o' 'build/main.o'
    if ($LASTEXITCODE) { throw 'Compilation failed' }
    & $gcc "-specs=$toolRoot/libnx/switch.specs" @arch '-Wl,-Map,build/app.map' 'build/main.o' "-L$toolRoot/libnx/lib" '-lnx' '-o' 'UltraBootscreenManager.elf'
    if ($LASTEXITCODE) { throw 'Linking failed' }
    & "$toolRoot/tools/bin/nacptool.exe" --create 'Ultra Bootscreen Manager' 'Jeronimo701' '1.1.1' 'UltraBootscreenManager.nacp'
    if ($LASTEXITCODE) { throw 'NACP generation failed' }
    & "$toolRoot/tools/bin/elf2nro.exe" 'UltraBootscreenManager.elf' 'UltraBootscreenManager.nro' '--nacp=UltraBootscreenManager.nacp' "--icon=$toolRoot/libnx/default_icon.jpg" '--romfsdir=romfs'
    if ($LASTEXITCODE) { throw 'NRO generation failed' }
    & (Join-Path $PSScriptRoot 'verify-nro.ps1')
} finally {
    Pop-Location
    $env:DEVKITPRO = $previousDevkit
}
