# KenshiTrainer build script (run from repo root)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$deps = Join-Path $root "reference\KenshiLib_Examples_deps"
$env:KENSHILIB_DIR = "$deps\KenshiLib"
$env:KENSHILIB_DEPS_DIR = $deps
$env:BOOST_INCLUDE_PATH = "$deps\boost_1_60_0"
$env:BOOST_ROOT = "$deps\boost_1_60_0"
$msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild (Join-Path $root "src\KenshiTrainer.vcxproj") /p:Configuration=Release /p:Platform=x64 /v:m /nologo
if ($LASTEXITCODE -ne 0) { throw "build failed" }
Copy-Item (Join-Path $root "src\x64\Release\KenshiTrainer.dll") (Join-Path $root "mods\KenshiTrainer\KenshiTrainer.dll") -Force
Write-Host "OK -> mods\KenshiTrainer\KenshiTrainer.dll"
