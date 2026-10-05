$ErrorActionPreference = 'Stop'
$testOutput = Join-Path $env:TEMP 'tarkov-input-tests'
New-Item -ItemType Directory -Force $testOutput | Out-Null
Push-Location $PSScriptRoot
try {
    # Run from Developer PowerShell for VS 2022 (x64).
    & cl.exe /nologo /std:c++17 /EHsc /MD /Istubs input_tests.cpp "/Fo$testOutput/input_tests.obj" "/Fe$testOutput/input_tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Input test compilation failed.' }
    & "$testOutput/input_tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Input regression tests failed.' }
}
finally {
    Pop-Location
}
