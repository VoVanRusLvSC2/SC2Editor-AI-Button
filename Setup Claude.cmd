@echo off
setlocal
cd /d "%~dp0bridge"
where node.exe >nul 2>&1 || (echo Install Node.js 18.14 or newer first.& pause & exit /b 1)
where npm.cmd >nul 2>&1 || (echo npm was not found. Install Node.js with npm.& pause & exit /b 1)
call npm.cmd ci --no-audit --no-fund
if errorlevel 1 (echo Claude SDK installation failed.& pause & exit /b 1)
echo Claude SDK installed. Run "Login Claude.cmd" to sign in.
pause
