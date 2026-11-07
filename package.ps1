param([string]$Version='0.7.0-alpha')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.-]+)?$') { throw 'Invalid package version.' }
& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE) { throw 'Build failed.' }
$destination = Join-Path $PSScriptRoot 'dist'
$stage = Join-Path $destination "SC2Editor-AI-Button-$Version"
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'build\SC2AIButton.dll'), (Join-Path $PSScriptRoot 'build\SC2AILoader.exe'), (Join-Path $PSScriptRoot 'Start AI Button.cmd'), (Join-Path $PSScriptRoot 'README.md'), (Join-Path $PSScriptRoot 'README.ru.md') -Destination $stage -Force
New-Item -ItemType Directory -Force (Join-Path $stage 'docs') | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'docs\chat-integration-plan.ru.md') -Destination (Join-Path $stage 'docs') -Force
$archive = Join-Path $destination "SC2Editor-AI-Button-win64-$Version.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Get-FileHash $archive -Algorithm SHA256 | Format-List
