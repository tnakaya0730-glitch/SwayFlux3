param([switch]$CoreOnly, [string]$CMake = 'cmake', [string]$DependencyPrefix = '')
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not (Get-Command $CMake -ErrorAction SilentlyContinue)) {
    $CMake = Join-Path $projectRoot 'tools/python/cmake/data/bin/cmake.exe'
}
if (-not (Get-Command $CMake -ErrorAction SilentlyContinue)) {
    throw 'CMake 3.24以上を用意するか、bootstrap-windows.ps1を実行してください'
}
$buildDirectory = Join-Path $projectRoot $(if ($CoreOnly) { 'build-core' } else { 'build' })
$options = @()
if ($CoreOnly) { $options += '-DSWAYFLUX3_BUILD_HTTP=OFF' }
else {
    if (-not $DependencyPrefix) { $DependencyPrefix = Join-Path $projectRoot 'deps/install' }
    $dependencyPath = (Resolve-Path -LiteralPath $DependencyPrefix).Path.Replace('\', '/')
    $options += @("-DCMAKE_PREFIX_PATH=$dependencyPath", "-DZLIB_LIBRARY=$dependencyPath/lib/zs.lib", "-DZLIB_INCLUDE_DIR=$dependencyPath/include")
}
& $CMake -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64 @options
if ($LASTEXITCODE -ne 0) { throw 'CMake構成に失敗しました' }
& $CMake --build $buildDirectory --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'ビルドに失敗しました' }
Write-Output "ビルド完了: $buildDirectory"
