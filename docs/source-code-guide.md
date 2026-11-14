# Source code guide

This guide is a map of the extension for people reading or changing its code. Comments in the
source explain individual decisions and Windows API constraints. Product setup is in the main
README; the AI window is an injected extension window, not a native Editor module.

## Main components

- `src/loader.cpp` locates a supported Editor, loads the DLL into that process, and asks it to
  install the toolbar button. It validates the executable and reports whether installation succeeded.
- `src/ai_button.cpp` owns the toolbar integration and the AI window. The `Context` structure
  keeps one toolbar's window handles, connection state, attachments, chat list, and transcript.
  Window procedures process native messages on the owning UI thread. Hooks and scans discover
  Editor windows; the GUI thread performs the actual toolbar changes.
- `src/bridge_client.*` starts the Node.js bridge, encodes fields for its line protocol, and posts
  decoded events from a reader thread back to the AI window. The bridge state remains alive
  until the reader and window release their references.
- `bridge/main.mjs` routes requests to the local Codex app-server or Claude Agent SDK. It tracks
  the active provider and chat, sends the current Editor context, handles streamed replies and
  permission questions, and rejects results belonging to an older connection generation.
- `bridge/mcp_refresh.mjs` requests an MCP reload when the local runtime fingerprint changes;
  `bridge/mcp_spawn.mjs` starts a configured MCP process when a working-directory wrapper is needed.
- `bridge/chat_titles.mjs` derives readable titles. `bridge/workspace_snapshot.mjs` and
  `bridge/file_changes.mjs` collect bounded text-file changes, store patches, and check file
  fingerprints before allowing undo. These do not track changes to the Editor's in-memory map.

## UI and data helpers

- `src/chat_markdown.h` parses the small Markdown subset displayed by the native chat.
  `src/chat_outline.h` derives short navigation labels from existing text; it does not call a model.
- `src/map_context.h` derives a candidate map path and unsaved marker from the Editor title and
  recent files. An inferred path does not prove that MCP is bound to the open in-memory document.
- `src/editor_window.h` and `src/editor_locale.h` identify Editor windows and choose the UI locale.
- `src/image_attachment.*` converts selected or pasted images to temporary PNGs and produces previews.
  The window owns and releases the preview bitmaps.
- `bridge/locales.json` is the translation source for the bridge. Run
  `python scripts/generate_locales.py` after changing it to regenerate `src/locales_generated.h`;
  edit the catalog and generator rather than hand-editing the generated header.

The transcript model is separate from the Rich Edit control. New entries mark a dirty suffix;
rendering replaces only that suffix, rebuilds its links and outline offsets, and restores the
visible line and selection. At the bottom the view follows new content. During scrollbar-thumb
dragging, rendering waits until tracking ends.

## Tests and formatting

`tests/` contains native UI and helper tests, fake providers, and Node.js bridge tests. Native GUI
tests that create windows use private desktops where implemented; the explicitly named live and
inspection utilities target a running Editor and should only be run intentionally. `build.ps1`
builds the x64 DLL and loader and runs the native test set. The Windows workflow in
`.github/workflows/windows-compat.yml` lists the bridge integration commands.

C++ formatting is configured in `.clang-format`; JavaScript uses `.prettierrc.json` and Prettier
3.6.2. Do not format `src/locales_generated.h` directly: the generator determines its exact text.
The Python generator is formatted with Black 24.10.0 at a 110-character line length.
