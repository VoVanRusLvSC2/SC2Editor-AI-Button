# SC2 Editor AI Button

An experimental Windows x64 extension for StarCraft II Editor 5.0.17.98310. It adds an AI toolbar button and opens a separate window with a chat, model selector, chat list, and settings for Codex or Claude. The AI button is selected only while the AI window is active and clicking it does not toggle the selection.

## Setup

1. Install Node.js 18.14 or newer with npm. Start the supported 64-bit Editor and open a map.
2. Extract the release and run `Start AI Button.cmd`, then click AI. Codex requires a compatible installed and signed-in Codex application or `codex.exe`.
3. Select Codex in AI settings and press “Sign in to Codex”. Complete sign-in with your ChatGPT account in the browser. An existing Codex desktop account is reused when available.
4. For Claude, run `Setup Claude.cmd`, sign in to Claude Console from AI settings, create an API key and set the Windows user environment variable `ANTHROPIC_API_KEY`. Restart the Editor. API usage is billed separately; Claude.ai subscription credentials cannot be used by this third-party Agent SDK application. Never send the key in chat.

Press **Enter** to send a message, or **Shift+Enter** to insert a new line. Below the message field, use the plus icon to attach images, the selector on the right to choose a model, and the arrow to send. The arrow becomes a stop icon during a response. Hover over an icon for its localized label. The transcript renders common Markdown formatting, HTTP(S) links and fenced code blocks with a copy action. Consecutive tool actions are collapsed into a clickable summary; expand it to inspect the details. Chat titles that contain only the internal editor context are displayed using the first user message when available; this does not rename the provider chat.

The AI toolbar exposes the Editor's New, Open, and Save commands, identified by the standard Ctrl+N, Ctrl+O, and Ctrl+S labels on the native toolbar. Their availability follows the native Editor; custom shortcuts or inaccessible labels may prevent discovery.

Game installation and user paths are discovered locally, and module windows are identified independently of their caption language. The executable version and toolbar layout are checked to avoid attaching to unsupported builds. Native tests have run on Windows 11 x64; automated builds also run on Windows Server 2022 and 2025. This does not establish compatibility with every Windows version, UI scale, or Editor language. Chats require compatible Node.js and Codex or Claude installations.

The DLL is loaded into the running Editor without modifying the game installation. If several Editors are open, run `SC2AILoader.exe --pid <id>`. AI is a separate extension window, not a registered native Editor module.

Chats appear on the left; choose “＋ New chat” in the list to start one. The send button changes to stop while a response is running. Codex lists locally available app-server threads; remote or cloud threads may not be accessible. Claude lists local Claude Code sessions, distinct from Claude website chats. See [Anthropic authentication rules](https://code.claude.com/docs/en/legal-and-compliance) and [API key setup](https://platform.claude.com/docs/en/get-api-key).

The extension supports English (enUS), Russian (ruRU), German (deDE), Spanish (esES/esMX), Brazilian Portuguese (ptBR), French (frFR), Italian (itIT), Korean (koKR), Polish (plPL), and Simplified and Traditional Chinese (zhCN/zhTW). It detects the editor locale from its menu and EditorVariables.txt, then falls back to the Windows language or English. Set SC2_AI_LOCALE before starting the Editor to override detection. UI translations are machine-assisted and have not been reviewed by native speakers for every language. User messages and AI replies are not translated; provider errors may remain in their original language. The conversation distinguishes roles and tool activity with color, and long command entries are shortened in the display.

Attach up to five images to a message by pasting into the input with Ctrl+V or selecting multiple files with the Image button. Each thumbnail has its own removal button. PNG, JPEG, GIF and BMP are converted to PNG, with a 7 MiB limit per image and 20 MiB combined after conversion. Sent images are retained as copies in the `SC2EditorAI` temporary folder for conversation history; an unsent attachment is removed when discarded or the window closes.

If another Codex process holds a chat for writing, continue that chat in the other client or start a new one in the Editor. When a send is rejected, the draft text and images stay in the composer.

Chats are grouped by Codex desktop projects, reflect their configured folder colors, and can be collapsed; Claude chats are grouped by working folder. Codex can edit files directly in the open map’s working directory; other locations and actions may require approval. The SC2 MCP live backend still works through saved checkpoints and cannot directly mutate the open Editor document in memory.

New chats created here are listed under **IN_EDITOR** by default. Use **All** to show the original groups and **IN_EDITOR** to filter again. Codex chats are also added to a custom sidebar section named **IN_EDITOR** in Codex; the map working directory remains unchanged. An already-open Codex window may need reopening to refresh changes made by the extension. Claude grouping remains local to this extension. Existing editor-created Codex chats without another sidebar section are synchronized too. **Delete** permanently deletes the selected provider chat and its child sessions after confirmation; in Codex this also removes the chat from the desktop app. Deleting a Claude session affects local files, not chats on the Claude website.

The assistant receives the open map path and Editor window identity with each message. Abbreviated titles are matched against recent Editor documents and marked as inferred; ambiguous paths are left unresolved. The configured `sc2editor` MCP server is also passed to Claude. Connectivity tests use read-only `live.windows` and capabilities; they do not require saving or `live.connect`. Updated MCP servers provide separate `live.memory.read/set` tools for direct edits of existing map-local Unit numeric fields, including unsaved values. The matching Data window must be selected; live Terrain, triggers and object creation are outside that backend. Only the separately selected `live.connect` checkpoint workflow requires a saved file and works through a private copy. If the memory tools are absent, reconnect the updated MCP frontend; saving does not load new tools. Restart this panel's bridge after a prompt update to load its new routing instructions. The map name shown in the UI does not by itself confirm an MCP connection. Approval requests are presented; some complex MCP elicitation forms are not yet supported and are declined. Long histories are truncated for display.

See [the implementation notes](docs/chat-integration-plan.ru.md). The Editor version, UI layouts, and scaling settings are not exhaustively tested.

## Author and use

The author is [VoVanRusLvSC2](https://github.com/VoVanRusLvSC2). You may use, copy, modify, and distribute the project, subject to retaining the notice and crediting the author in public mentions or publications. Contact the author on GitHub with questions. See [LICENSE](LICENSE) for the full terms.
### Current-document edits and MCP updates

Map prompts default to the currently open document, followed by a requested
native same-file save and an actual change summary. Checkpoint/Save As copies
require explicit user selection; unsupported live operations must not silently
fall back to another map. Current MCP live editing supports existing Unit numeric
fields and native document Save. Direct placement/Terrain/trigger editing is not
implemented by this policy.

Before a Codex turn, the bridge fingerprints the configured local server entry
and live module files. A changed fingerprint queues MCP configuration refresh on
this panel's own app-server, before starting/resuming the turn. It never kills
Editor, game or other chats' servers. A refresh acknowledgement is not capability
verification: inspect `live.capabilities.apiVersion` and actual tools.
After replacing an already running bridge script, reopen the AI panel once; its
Node process cannot retroactively load changed source. Editor may remain open.
