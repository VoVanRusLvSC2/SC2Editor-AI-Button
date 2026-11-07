@echo off
setlocal
set "CLAUDE_BIN=%~dp0bridge\node_modules\@anthropic-ai\claude-agent-sdk-win32-x64\claude.exe"
if not exist "%CLAUDE_BIN%" (echo First run "Setup Claude.cmd".& pause & exit /b 1)
"%CLAUDE_BIN%" auth login
if errorlevel 1 (echo Claude sign-in did not complete.& pause & exit /b 1)
echo Sign-in complete. Switch away from Claude and back in AI settings to reconnect.
pause
