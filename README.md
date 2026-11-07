# SC2 Editor AI Button

An experimental Windows x64 extension for the original StarCraft II Editor.
It adds a steel-blue **AI** icon the same size as the Editor's other toolbar
buttons, before the SC2 button. Where the standard module toolbar is detected,
the lettering uses the Editor's own Text icon artwork and colors. AI opens a
separate window with its own selected AI button and a toolbar for switching to
Editor modules. The window previews a central chat, model selector, chat list
and settings for Codex / Claude. The chat, model selector and MCP are not yet
connected. See [the integration plan](docs/chat-integration-plan.ru.md).

## Use

1. Run the original 64-bit StarCraft II Editor and open a map.
2. Extract the release ZIP to a folder you control.
3. Run `Start AI Button.cmd`. The DLL and loader must remain in the same folder.
4. Click **AI** in the Editor toolbar. Re-run the launcher after restarting the Editor.

The launcher loads the DLL into the running Editor process; no game files are
modified. The DLL remains loaded until that Editor exits. The supported Editor
version is **5.0.17.98310 x64**. An unrecognized version or toolbar layout fails
without inserting a button. If several visible Editor processes are running,
select one explicitly in a console with `SC2AILoader.exe --pid <process-id>`.

This is an experimental extension. AI is a separate extension window, not a
registered native Editor module. The AI toolbar supports module navigation;
unrelated editing buttons are disabled. If the Editor toolbar is too narrow, its
usual overflow behavior may hide buttons. Make a backup of important work and
test with a disposable map before using it for real editing.

## Build and verify

Run `powershell -ExecutionPolicy Bypass -File .\build.ps1` with Visual Studio C++
x64 tools and the Windows SDK installed. It builds the DLL, launcher and tests,
then runs the isolated toolbar/window test. `package.ps1` creates a ZIP in `dist`.

Live validation is separate: on an isolated Editor 5.0.17.98310 and a disposable
map, the button was inserted into the original Terrain and Triggers toolbars,
and mouse messages opened, closed and reopened the AI window; navigation from AI to the native Data module was also exercised. The Editor's
other builds, toolbar arrangements, scaling settings and all modules are not
yet exhaustively validated.

The DLL exports `SC2AI_Attach`, `SC2AI_Detach`, `SC2AI_Start` and `SC2AI_Status`.
Direct attach/detach calls must execute on the toolbar's UI thread; the shipped
launcher initializes the DLL and lets it locate compatible Editor toolbars.
