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
Run-Native $Compiler ($flags + @('-c',(Join-Path $PSScriptRoot 'src/native_dan.cpp'),'-o',(Join-Path $build 'native_dan.o')))
Run-Native $Compiler ($flags + @('-shared') + @((Join-Path $PSScriptRoot 'src/native_runtime.cpp'),(Join-Path $build 'native_dan.o')) + @('-o',(Join-Path $build '2dx-free-dan.dll')))
if (-not $SkipTests) {
    foreach ($test in @('course','session','folder','selector','selection','calls','result','binding','runtime','compatibility','options')) {
        $objects = @()
        if ($test -in @('binding','runtime','compatibility','options')) { $objects = @((Join-Path $build 'native_dan.o')) }
        Run-Native $Compiler ($flags + @((Join-Path $PSScriptRoot "tests/${test}_test.cpp")) + $objects + @('-o',(Join-Path $build "${test}_test.exe")))
        Run-Native (Join-Path $build "${test}_test.exe") @()
    }
    if ($GameRoot) {
        $GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
        foreach ($test in @('current_bindings','current_options')) {
            Run-Native $Compiler ($flags + @((Join-Path $PSScriptRoot "tests/${test}_test.cpp")) + @((Join-Path $build 'native_dan.o')) + @('-o',(Join-Path $build "${test}_test.exe")))
            Push-Location -LiteralPath $GameRoot
            try { Run-Native (Join-Path $build "${test}_test.exe") @() } finally { Pop-Location }
        }
    }
}
Write-Host 'Built 2dx-free-dan.dll; requested tests passed.'
