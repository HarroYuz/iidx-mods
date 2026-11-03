param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$Compiler = 'g++',
    [string]$Python = 'python',
    [switch]$Firenet,
    [string]$PracticeRoot = (Join-Path (Split-Path (Split-Path $PSScriptRoot)) '2dx-practice')
)
$ErrorActionPreference = 'Stop'
$freeRoot = Split-Path $PSScriptRoot
$build = Join-Path $freeRoot 'build/cooperation'
New-Item -ItemType Directory -Force -Path $build | Out-Null
function Run-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed (exit $LASTEXITCODE)" }
}
if ($Firenet) { Run-Native $Python @((Join-Path $PSScriptRoot 'firenet_audit.py'),'--game-root',$GameRoot) }
$flags = @('-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-static-libgcc','-static-libstdc++')
Run-Native $Compiler ($flags + @('-shared',(Join-Path $PSScriptRoot 'cooperation_dll.cpp'),(Join-Path $freeRoot 'src/native_dan.cpp'),'-o',(Join-Path $build '2dx-free-dan.dll')))
Run-Native $Compiler ($flags + @('-shared',(Join-Path $PracticeRoot 'tests/cooperation_dll.cpp'),'-o',(Join-Path $build '2dx-practice.dll')))
Run-Native $Compiler ($flags + @((Join-Path $PSScriptRoot 'cooperation_test.cpp'),'-o',(Join-Path $build 'cooperation_test.exe')))
$profile = Get-Content -LiteralPath (Join-Path $PracticeRoot 'addresses/0917.json') -Raw | ConvertFrom-Json
$previousTestRvas = $env:IIDX_MODS_TEST_RVAS
$env:IIDX_MODS_TEST_RVAS = (@('0xa90400','0x5c5300','0xab7b80','0x82d250') | ForEach-Object { $profile.addresses.$_.rva }) -join ','
Push-Location -LiteralPath $GameRoot
try {
    foreach ($order in @('forward','reverse')) {
        $testArgs = @($build,$order)
        if ($Firenet) { $testArgs += 'firenet' }
        Run-Native (Join-Path $build 'cooperation_test.exe') $testArgs
    }
} finally {
    Pop-Location
    $env:IIDX_MODS_TEST_RVAS = $previousTestRvas
}
