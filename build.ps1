# =============================================================================
#  Lake Fishing 3D - one-click build script for Windows (PowerShell)
#
#  Usage:   .\build.ps1                 # Release build
#           .\build.ps1 -Config Debug   # Debug build (console window, symbols)
#           .\build.ps1 -Run            # build and start the game
#
#  Uses CMake + Ninja + MinGW from PATH; if they are not on PATH it falls
#  back to the toolchain bundled with JetBrains CLion (auto-detected).
# =============================================================================
param(
    [ValidateSet("Release", "Debug")] [string]$Config = "Release",
    [switch]$Run
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-Tool($name) { $c = Get-Command $name -ErrorAction SilentlyContinue; if ($c) { return $c.Source } return $null }

if (-not (Find-Tool "g++") -or -not (Find-Tool "cmake")) {
    $clion = Get-ChildItem "C:\Program Files\JetBrains" -Directory -Filter "CLion*" -ErrorAction SilentlyContinue |
             Sort-Object Name -Descending | Select-Object -First 1
    if ($clion) {
        $bin = Join-Path $clion.FullName "bin"
        $env:PATH = "$bin\mingw\bin;$bin\cmake\win\x64\bin;$bin\ninja\win\x64;$env:PATH"
        Write-Host "Using toolchain bundled with $($clion.Name)"
    }
}
if (-not (Find-Tool "cmake")) { throw "CMake not found. Install CMake + MinGW-w64 (or CLion) and retry." }

$build = Join-Path $root ("build-" + $Config.ToLower())
$gen = @()
if (-not (Test-Path -LiteralPath (Join-Path $build "CMakeCache.txt"))) {
    if (Find-Tool "ninja") { $gen = @("-G", "Ninja") } elseif (Find-Tool "mingw32-make") { $gen = @("-G", "MinGW Makefiles") }
}

cmake -S $root -B $build @gen "-DCMAKE_BUILD_TYPE=$Config"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$exe = Join-Path $build "bin\LakeFishing3D.exe"
Write-Host "`nBuild OK: $exe"
if ($Run) { Start-Process $exe -WorkingDirectory (Split-Path $exe) }
