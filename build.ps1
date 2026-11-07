$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ x64 tools are required.' }
$msvc = (Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$version = (Get-ChildItem (Join-Path $sdk 'Include') -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'um\windows.h') } | Sort-Object Name -Descending | Select-Object -First 1).Name
if (-not $version) { throw 'Windows SDK is required.' }
$env:PATH = (Join-Path $msvc 'bin\Hostx64\x64') + ';' + $env:PATH
$env:INCLUDE = @((Join-Path $msvc 'include'), (Join-Path $sdk "Include\$version\ucrt"), (Join-Path $sdk "Include\$version\shared"), (Join-Path $sdk "Include\$version\um")) -join ';'
$env:LIB = @((Join-Path $msvc 'lib\x64'), (Join-Path $sdk "Lib\$version\ucrt\x64"), (Join-Path $sdk "Lib\$version\um\x64")) -join ';'
$output = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $output | Out-Null
Push-Location $output
try {
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT /LD (Join-Path $PSScriptRoot 'src\ai_button.cpp') /Fe:SC2AIButton.dll user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'DLL build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\toolbar_test.cpp') /Fe:toolbar_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Test build failed.' }
    & .\toolbar_test.exe
    if ($LASTEXITCODE) { throw 'Toolbar test failed.' }
} finally { Pop-Location }
