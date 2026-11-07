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
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT /DUNICODE /D_UNICODE /LD (Join-Path $PSScriptRoot 'src\ai_button.cpp') (Join-Path $PSScriptRoot 'src\bridge_client.cpp') (Join-Path $PSScriptRoot 'src\image_attachment.cpp') /Fe:SC2AIButton.dll user32.lib comctl32.lib gdi32.lib gdiplus.lib crypt32.lib shell32.lib advapi32.lib comdlg32.lib oleacc.lib oleaut32.lib ole32.lib
    if ($LASTEXITCODE) { throw 'DLL build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'src\loader.cpp') /Fe:SC2AILoader.exe user32.lib version.lib
    if ($LASTEXITCODE) { throw 'Loader build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\toolbar_test.cpp') /Fe:toolbar_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\file_toolbar_test.cpp') /Fe:file_toolbar_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'File toolbar test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\overview_toolbar_test.cpp') /Fe:overview_toolbar_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Overview toolbar test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\editor_window_test.cpp') /Fe:editor_window_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Editor window test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\map_context_test.cpp') /Fe:map_context_test.exe user32.lib advapi32.lib
    if ($LASTEXITCODE) { throw 'Map context test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\image_attachment_test.cpp') (Join-Path $PSScriptRoot 'src\image_attachment.cpp') /Fe:image_attachment_test.exe user32.lib gdi32.lib gdiplus.lib shell32.lib
    if ($LASTEXITCODE) { throw 'Image attachment test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\image_clipboard_test.cpp') (Join-Path $PSScriptRoot 'src\image_attachment.cpp') /Fe:image_clipboard_test.exe user32.lib gdi32.lib gdiplus.lib shell32.lib
    if ($LASTEXITCODE) { throw 'Image clipboard test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\image_ui_test.cpp') /Fe:image_ui_test.exe user32.lib comctl32.lib gdi32.lib
    if ($LASTEXITCODE) { throw 'Image UI test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\collision_ui_test.cpp') /Fe:collision_ui_test.exe user32.lib comctl32.lib gdi32.lib
    if ($LASTEXITCODE) { throw 'Collision UI test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\chat_color_test.cpp') /Fe:chat_color_test.exe user32.lib comctl32.lib gdi32.lib
    if ($LASTEXITCODE) { throw 'Chat color test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\group_ui_test.cpp') /Fe:group_ui_test.exe user32.lib comctl32.lib gdi32.lib
    if ($LASTEXITCODE) { throw 'Group UI test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\inspect_toolbar.cpp') /Fe:inspect_toolbar.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Inspector build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\live_click.cpp') /Fe:live_click.exe user32.lib comctl32.lib gdi32.lib
    if ($LASTEXITCODE) { throw 'Live test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\ai_bridge_debug.cpp') /Fe:ai_bridge_debug.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'AI bridge debug build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\ai_bridge_live.cpp') /Fe:ai_bridge_live.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'AI bridge live test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\ai_switch_test.cpp') /Fe:ai_switch_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'AI module-switch test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\switch_module.cpp') /Fe:switch_module.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Module-switch test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\legacy_upgrade_test.cpp') /Fe:legacy_upgrade_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Legacy upgrade test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\late_legacy_test.cpp') /Fe:late_legacy_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Late legacy test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\diagnose_editor.cpp') /Fe:diagnose_editor.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Editor diagnostics build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /MT (Join-Path $PSScriptRoot 'tests\multi_upgrade_test.cpp') /Fe:multi_upgrade_test.exe user32.lib comctl32.lib
    if ($LASTEXITCODE) { throw 'Multi-version upgrade test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\locale_ui_test.cpp') /Fe:locale_ui_test.exe user32.lib comctl32.lib shell32.lib
    if ($LASTEXITCODE) { throw 'Locale UI test build failed.' }
    & cl.exe /nologo /W4 /WX /std:c++17 /EHsc /utf-8 /MT (Join-Path $PSScriptRoot 'tests\chat_markdown_test.cpp') /Fe:chat_markdown_test.exe
    if ($LASTEXITCODE) { throw 'Markdown test build failed.' }
    & .\chat_markdown_test.exe
    if ($LASTEXITCODE) { throw 'Markdown test failed.' }
    & .\chat_color_test.exe
    if ($LASTEXITCODE) { throw 'Chat display test failed.' }
    & .\locale_ui_test.exe
    if ($LASTEXITCODE) { throw 'Locale UI test failed.' }
    & .\toolbar_test.exe
    if ($LASTEXITCODE) { throw 'Toolbar test failed.' }
    & .\file_toolbar_test.exe
    if ($LASTEXITCODE) { throw 'File toolbar test failed.' }
    & .\editor_window_test.exe
    if ($LASTEXITCODE) { throw 'Editor window test failed.' }
    & .\map_context_test.exe
    if ($LASTEXITCODE) { throw 'Map context test failed.' }
    & .\image_attachment_test.exe
    if ($LASTEXITCODE) { throw 'Image attachment test failed.' }
} finally { Pop-Location }
