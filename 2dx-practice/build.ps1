param(
    [string]$Compiler = 'g++',
    [string]$Python = 'python',
    [string]$GameRoot = '',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
function Run-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed (exit $LASTEXITCODE)" }
}
Run-Native $Python @((Join-Path $PSScriptRoot 'tools/generate_addresses.py'))
Run-Native $Python @((Join-Path $PSScriptRoot 'tests/assets_test.py'))
$flags = @('-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-static-libgcc','-static-libstdc++')

Run-Native $Compiler ($flags + @('-shared') + @((Join-Path $PSScriptRoot 'src/runtime.cpp')) + @('-o',(Join-Path $build '2dx-practice.dll')))
if (-not $SkipTests) {
    foreach ($test in @('core')) {
        $objects = @()

        Run-Native $Compiler ($flags + @((Join-Path $PSScriptRoot "tests/${test}_test.cpp")) + $objects + @('-o',(Join-Path $build "${test}_test.exe")))
        Run-Native (Join-Path $build "${test}_test.exe") @()
    }
    if ($GameRoot) {
        $GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
        foreach ($test in @('native_audio','ingame_rate','corpus')) {
            Run-Native $Compiler ($flags + @((Join-Path $PSScriptRoot "tests/${test}_test.cpp")) + @() + @('-o',(Join-Path $build "${test}_test.exe")))
            Push-Location -LiteralPath $GameRoot
            try { Run-Native (Join-Path $build "${test}_test.exe") @() } finally { Pop-Location }
        }
    }
}
Write-Host 'Built 2dx-practice.dll; requested tests passed.'
