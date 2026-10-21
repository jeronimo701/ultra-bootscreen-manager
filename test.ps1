param([Parameter(Mandatory=$true)][string]$Zig)
$ErrorActionPreference = 'Stop'
$compiler = (Resolve-Path -LiteralPath $Zig).Path
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path 'build' -Force | Out-Null
    & $compiler cc -O2 -Wall -Wextra -Itests/include tests/native.c -o build/native-tests.exe
    if ($LASTEXITCODE) { throw 'Native test compilation failed' }
    & .\build\native-tests.exe
    if ($LASTEXITCODE) { throw 'Native tests failed' }
    Get-Content -LiteralPath 'verification/native/results.txt'
} finally { Pop-Location }
