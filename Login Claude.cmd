@echo off
setlocal
rem Open the provider's official API key page; the script never reads the key.
start "" "https://platform.claude.com/settings/keys"
echo Sign in to Claude Console and create an API key.
echo Set ANTHROPIC_API_KEY as a user environment variable, then restart the Editor.
echo API usage is billed separately. Never paste the key into chat.
pause
