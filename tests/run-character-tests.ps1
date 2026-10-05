param([string]$GlmInclude = '')
$ErrorActionPreference = 'Stop'
$testOutput = Join-Path $env:TEMP 'tarkov-character-tests'
New-Item -ItemType Directory -Force $testOutput | Out-Null
if (-not $GlmInclude) {
    $installedGlm = Join-Path $PSScriptRoot '../vcpkg_installed/x64-windows/include'
    if (Test-Path (Join-Path $installedGlm 'glm/vec3.hpp')) {
        $GlmInclude = $installedGlm
    }
    else {
        $GlmInclude = Join-Path $PSScriptRoot 'stubs'
        Write-Host 'Using test vector stubs; real GLM integration is not checked.'
    }
}
Push-Location $PSScriptRoot
try {
    & cl.exe /nologo /std:c++17 /EHsc /MD "/I$GlmInclude" character_tests.cpp "/Fo$testOutput/character_tests.obj" "/Fe$testOutput/character_tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Character test compilation failed.' }
    & "$testOutput/character_tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Character regression tests failed.' }
}
finally {
    Pop-Location
}
