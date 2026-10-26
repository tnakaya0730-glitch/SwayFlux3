param([string]$CMake = 'cmake')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$deps = (Join-Path $root 'deps').Replace('\', '/')
$prefix = (Join-Path $deps 'install').Replace('\', '/')
New-Item -ItemType Directory -Force $deps | Out-Null

# PATHにCMakeがなければ、このプロジェクト内だけに取得します。
if (-not (Get-Command $CMake -ErrorAction SilentlyContinue)) {
    $localTool = Join-Path $root 'tools/python/cmake/data/bin/cmake.exe'
    if (-not (Test-Path -LiteralPath $localTool)) {
        python -m pip install --target (Join-Path $root 'tools/python') cmake==4.4.4
        if ($LASTEXITCODE -ne 0) { throw 'CMakeの取得に失敗しました' }
    }
    $CMake = $localTool
}
function Run-CMake {
    & $CMake @args
    if ($LASTEXITCODE -ne 0) { throw "CMake失敗: $args" }
}
function Get-Source([string]$Name, [string]$Url, [string]$Hash) {
    $archive = Join-Path $deps "$Name.tar.gz"
    if (-not (Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $Url -OutFile $archive }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Hash) {
        throw "SHA256が一致しません: $Name"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $deps $Name))) {
        # zstdのCLIテスト用symlinkはWindowsで不要です。
        tar --exclude="$Name/tests" -xf $archive -C $deps
        if ($LASTEXITCODE -ne 0) { throw "展開失敗: $Name" }
    }
}
function Build-Dependency([string]$Name, [string]$Source, [string[]]$Options) {
    $build = Join-Path $deps "$Name-build"
    Run-CMake -S $Source -B $build -G 'Visual Studio 17 2022' -A x64 `
        "-DCMAKE_INSTALL_PREFIX=$prefix" "-DCMAKE_PREFIX_PATH=$prefix" @Options
    Run-CMake --build $build --config Release --target install --parallel 4
}
Get-Source 'zlib-1.3.2' 'https://zlib.net/zlib-1.3.2.tar.gz' 'BB329A0A2CD0274D05519D61C667C062E06990D72E125EE2DFA8DE64F0119D16'
Get-Source 'zstd-1.5.7' 'https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz' 'EB33E51F49A15E023950CD7825CA74A4A2B43DB8354825AC24FC1B7EE09E6FA3'
Get-Source 'brotli-1.2.1' 'https://github.com/google/brotli/archive/refs/tags/v1.2.1.tar.gz' '98D35D11A78DC453AEBB7D10C3748F3F73FF78C8267C0DC697B8631F67BF7E8F'
# curlはtar.xzですが、tarはファイル名に依存せず形式を識別します。
Get-Source 'curl-8.23.0' 'https://curl.se/download/curl-8.23.0.tar.xz' '8DADD7D2536A314F982AD05A651B713F426BF7D02CC2C76CFFB9796F2218E6C5'
Build-Dependency 'zlib' (Join-Path $deps 'zlib-1.3.2') @('-DZLIB_BUILD_SHARED=OFF', '-DZLIB_BUILD_TESTING=OFF')
Build-Dependency 'zstd' (Join-Path $deps 'zstd-1.5.7/build/cmake') @('-DZSTD_BUILD_PROGRAMS=OFF', '-DZSTD_BUILD_TESTS=OFF', '-DZSTD_BUILD_SHARED=OFF', '-DZSTD_MULTITHREAD_SUPPORT=OFF')
Build-Dependency 'brotli' (Join-Path $deps 'brotli-1.2.1') @('-DBUILD_SHARED_LIBS=OFF', '-DBROTLI_DISABLE_TESTS=ON')
Build-Dependency 'curl' (Join-Path $deps 'curl-8.23.0') @('-DBUILD_SHARED_LIBS=OFF', '-DBUILD_STATIC_LIBS=ON', '-DBUILD_CURL_EXE=OFF', '-DBUILD_TESTING=OFF', '-DCURL_USE_SCHANNEL=ON', '-DCURL_USE_OPENSSL=OFF', '-DCURL_ZLIB=ON', '-DCURL_BROTLI=ON', '-DCURL_ZSTD=ON', '-DZSTD_USE_STATIC_LIBS=ON', '-DUSE_NGHTTP2=OFF', '-DCURL_USE_LIBPSL=OFF', "-DZLIB_LIBRARY=$prefix/lib/zs.lib", "-DZLIB_INCLUDE_DIR=$prefix/include")
Write-Host "依存ライブラリを配置しました: $prefix"
