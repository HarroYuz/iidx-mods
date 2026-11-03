param([Parameter(Mandatory=$true)][string]$GameRoot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$game = Join-Path $root 'bm2dx.dll'
if (Get-Process spice64,spice -ErrorAction SilentlyContinue) { throw 'Exit the game before installing.' }
$bytes = [IO.File]::ReadAllBytes($game)
$pe = [BitConverter]::ToInt32($bytes,60)
$stamp = [BitConverter]::ToUInt32($bytes,$pe+8)
$entry = [BitConverter]::ToUInt32($bytes,$pe+24+16)
$size = [BitConverter]::ToUInt32($bytes,$pe+24+56)
$profile = @('0917') | ForEach-Object {
    Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot "addresses/$_.json") | ConvertFrom-Json
} | Where-Object {
    [Convert]::ToUInt32($_.timestamp.Substring(2),16) -eq $stamp -and
    [Convert]::ToUInt32($_.entry.Substring(2),16) -eq $entry -and
    [Convert]::ToUInt32($_.image_size.Substring(2),16) -eq $size
}
if (@($profile).Count -ne 1) { throw 'Unsupported game: installation cancelled.' }
if (-not (Test-Path -LiteralPath (Join-Path $root 'ifs_hook.dll'))) { throw 'ifs_hook.dll is required.' }
$dll = Join-Path $PSScriptRoot 'build/2dx-practice.dll'
if (-not (Test-Path -LiteralPath $dll)) { $dll = Join-Path (Split-Path -Parent $PSScriptRoot) 'build/bin/2dx-practice.dll' }
if (-not (Test-Path -LiteralPath $dll)) { throw 'Build with CMake or build.ps1 first.' }
$assets = Join-Path (Split-Path -Parent $PSScriptRoot) 'data_mods/practice'
if (-not (Test-Path -LiteralPath $assets)) { throw 'The repository data_mods resources are required.' }
$destination = [IO.Path]::GetFullPath((Join-Path $root 'data_mods/practice'))
if (-not $destination.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe asset destination.' }
$backup = Join-Path $root ('mod-backups/2dx-practice-'+(Get-Date -Format 'yyyyMMdd-HHmmss-ffff'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
if (Test-Path -LiteralPath $destination) {
    $resolved = (Resolve-Path -LiteralPath $destination).Path
    if ($resolved -ne $destination -or -not $resolved.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe resolved asset path.' }
    Move-Item -LiteralPath $resolved -Destination (Join-Path $backup 'practice')
}
$installed = Join-Path $root '2dx-practice.dll'
if (Test-Path -LiteralPath $installed) { Copy-Item -LiteralPath $installed -Destination $backup }
New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
Copy-Item -LiteralPath $assets -Destination $destination -Recurse
Copy-Item -LiteralPath $dll -Destination $installed -Force
Write-Host "Installed 2dx-practice for $($profile.name). Backup: $backup"
Write-Host 'Load with: -k ifs_hook.dll -k 2dx-practice.dll'
