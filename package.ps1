param([string]$Version='0.31.1-alpha')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.-]+)?$') { throw 'Invalid package version.' }
& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE) { throw 'Build failed.' }
$destination = Join-Path $PSScriptRoot 'dist'
$stage = Join-Path $destination "SC2Editor-AI-Button-$Version"
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'build\SC2AIButton.dll'), (Join-Path $PSScriptRoot 'build\SC2AILoader.exe'), (Join-Path $PSScriptRoot 'Start AI Button.cmd'), (Join-Path $PSScriptRoot 'README.md'), (Join-Path $PSScriptRoot 'README.ru.md'), (Join-Path $PSScriptRoot 'LICENSE'), (Join-Path $PSScriptRoot 'Setup Claude.cmd'), (Join-Path $PSScriptRoot 'Login Claude.cmd') -Destination $stage -Force
New-Item -ItemType Directory -Force (Join-Path $stage 'docs') | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'docs\chat-integration-plan.ru.md') -Destination (Join-Path $stage 'docs') -Force
New-Item -ItemType Directory -Force (Join-Path $stage 'bridge') | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'bridge\main.mjs'), (Join-Path $PSScriptRoot 'bridge\chat_titles.mjs'), (Join-Path $PSScriptRoot 'bridge\locales.json'), (Join-Path $PSScriptRoot 'bridge\mcp_spawn.mjs'), (Join-Path $PSScriptRoot 'bridge\mcp_refresh.mjs'), (Join-Path $PSScriptRoot 'bridge\package.json'), (Join-Path $PSScriptRoot 'bridge\package-lock.json') -Destination (Join-Path $stage 'bridge') -Force
$archive = Join-Path $destination "SC2Editor-AI-Button-win64-$Version.zip"
# Only ship declared files: a local Claude SDK installation can be very large.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$entries = @('SC2AIButton.dll','SC2AILoader.exe','Start AI Button.cmd','Setup Claude.cmd','Login Claude.cmd','README.md','README.ru.md','LICENSE','docs\chat-integration-plan.ru.md','bridge\main.mjs','bridge\chat_titles.mjs','bridge\locales.json','bridge\mcp_spawn.mjs','bridge\mcp_refresh.mjs','bridge\package.json','bridge\package-lock.json')
$stream = [IO.File]::Open($archive,[IO.FileMode]::Create)
$zip = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try { foreach ($relative in $entries) { [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $stage $relative),$relative.Replace('\','/'),[IO.Compression.CompressionLevel]::Optimal) } }
finally { $zip.Dispose(); $stream.Dispose() }
Get-FileHash $archive -Algorithm SHA256 | Format-List
