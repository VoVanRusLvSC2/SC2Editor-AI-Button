# Build and package only the files declared for a Windows release.
param([string]$Version = '0.34.2-alpha')

$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.-]+)?$') {
    throw 'Invalid package version.'
}

& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE) {
    throw 'Build failed.'
}

$destination = Join-Path $PSScriptRoot 'dist'
$stage = Join-Path $destination "SC2Editor-AI-Button-$Version"
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage 'docs') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage 'bridge') | Out-Null

# Stage the binaries, startup scripts and documentation.
$rootFiles = @(
    'build\SC2AIButton.dll'
    'build\SC2AILoader.exe'
    'Start AI Button.cmd'
    'README.md'
    'README.ru.md'
    'LICENSE'
    'Setup Claude.cmd'
    'Login Claude.cmd'
)
foreach ($relative in $rootFiles) {
    Copy-Item (Join-Path $PSScriptRoot $relative) -Destination $stage -Force
}
$docFiles = @('chat-integration-plan.ru.md', 'source-code-guide.md')
foreach ($name in $docFiles) {
    Copy-Item (Join-Path $PSScriptRoot "docs\$name") -Destination (Join-Path $stage 'docs') -Force
}
$bridgeFiles = @(
    'main.mjs'
    'chat_titles.mjs'
    'file_changes.mjs'
    'workspace_snapshot.mjs'
    'locales.json'
    'mcp_spawn.mjs'
    'mcp_refresh.mjs'
    'package.json'
    'package-lock.json'
)
foreach ($name in $bridgeFiles) {
    Copy-Item (Join-Path $PSScriptRoot "bridge\$name") -Destination (Join-Path $stage 'bridge') -Force
}

# Only ship declared files: a local Claude SDK installation can be very large.
$entries = @(
    'SC2AIButton.dll'
    'SC2AILoader.exe'
    'Start AI Button.cmd'
    'Setup Claude.cmd'
    'Login Claude.cmd'
    'README.md'
    'README.ru.md'
    'LICENSE'
    'docs\chat-integration-plan.ru.md'
    'docs\source-code-guide.md'
    'bridge\main.mjs'
    'bridge\chat_titles.mjs'
    'bridge\file_changes.mjs'
    'bridge\workspace_snapshot.mjs'
    'bridge\locales.json'
    'bridge\mcp_spawn.mjs'
    'bridge\mcp_refresh.mjs'
    'bridge\package.json'
    'bridge\package-lock.json'
)
$archive = Join-Path $destination "SC2Editor-AI-Button-win64-$Version.zip"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($archive, [IO.FileMode]::Create)
$zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($relative in $entries) {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip,
            (Join-Path $stage $relative),
            $relative.Replace('\', '/'),
            [IO.Compression.CompressionLevel]::Optimal
        )
    }
}
finally {
    $zip.Dispose()
    $stream.Dispose()
}
Get-FileHash $archive -Algorithm SHA256 | Format-List
