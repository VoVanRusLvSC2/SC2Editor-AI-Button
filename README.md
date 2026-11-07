# SC2 Editor AI Button — experimental

An early Windows x64 DLL prototype for a future AI button in StarCraft II Editor.
The current DLL inserts an `AI` text button into a caller-supplied Win32 toolbar.
It has been designed for an isolated toolbar test; **it does not yet locate or
attach to the SC2 Editor**. It has no loader or chat, and does not edit maps.

Build and run the isolated test with `powershell -ExecutionPolicy Bypass -File .\build.ps1`.
The build needs Visual Studio C++ x64 tools and the Windows SDK. The build output
is in `build/` and is not committed.

The caller must load the DLL in the toolbar's process, invoke `SC2AI_Attach` on
the toolbar's UI thread, supply the insertion index and an unused 16-bit command
ID, then call `SC2AI_Detach` on the same thread before unloading the DLL. The
toolbar owner receives the usual command notification on a click. The exports do
not identify the correct editor window, choose the command ID, or route clicks.
These integration steps and editor version compatibility remain unverified.
