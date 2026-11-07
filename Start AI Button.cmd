@echo off
cd /d "%~dp0"
"%~dp0SC2AILoader.exe"
if errorlevel 1 (
  echo.
  echo Could not confirm the AI button. Check the message above.
  pause
  exit /b 1
)
echo.
echo Click AI on the Editor toolbar.
pause
