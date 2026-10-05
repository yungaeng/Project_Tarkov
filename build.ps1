param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = 'Stop'
if (-not $VcpkgRoot) { $VcpkgRoot = Join-Path $env:USERPROFILE 'vcpkg' }
if (-not (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
    throw 'vcpkg.exe가 필요합니다. VCPKG_ROOT 또는 -VcpkgRoot로 설치 경로를 지정하세요.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'Visual Studio C++ 빌드 도구를 찾을 수 없습니다.' }

# Normalize the casing: MSBuild cannot launch CL when PATH and Path coexist.
$savedPath = $env:PATH
$savedPkgConfig = $env:PKG_CONFIG
$savedKeepVars = $env:VCPKG_KEEP_ENV_VARS
try {
    # Reuse a local standalone pkg-config when an older vcpkg's MSYS mirror is unavailable.
    $localPkgConfig = Join-Path $PSScriptRoot '.build/pkgconf/mingw64/bin/pkg-config.exe'
    if (-not $env:PKG_CONFIG -and (Test-Path $localPkgConfig)) {
        $env:PKG_CONFIG = $localPkgConfig
        $env:VCPKG_KEEP_ENV_VARS = (@($savedKeepVars, 'PKG_CONFIG') | Where-Object { $_ }) -join ';'
    }
    [Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
    [Environment]::SetEnvironmentVariable('Path', $null, 'Process')
    [Environment]::SetEnvironmentVariable('Path', $savedPath, 'Process')
    & $msbuild (Join-Path $PSScriptRoot 'Project_Tarkov/Project_Tarkov.sln') /m /restore /p:RestorePackagesConfig=true "/p:Configuration=$Configuration" /p:Platform=x64 "/p:VcpkgRoot=$VcpkgRoot" /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "빌드 실패 (종료 코드: $LASTEXITCODE)" }
}
finally {
    [Environment]::SetEnvironmentVariable('Path', $savedPath, 'Process')
    $env:PKG_CONFIG = $savedPkgConfig
    $env:VCPKG_KEEP_ENV_VARS = $savedKeepVars
}
