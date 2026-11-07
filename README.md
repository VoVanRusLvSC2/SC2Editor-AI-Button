# SC2 Editor AI Button

An experimental Windows x64 extension for StarCraft II Editor 5.0.17.98310. It adds an AI toolbar button and opens a separate window with a chat, model selector, chat list, and settings for Codex or Claude. The AI button is selected only while the AI window is active and clicking it does not toggle the selection.

## Setup

1. Install Node.js 18.14 or newer with npm. Start the supported 64-bit Editor and open a map.
2. Extract the release and run `Start AI Button.cmd`, then click AI. Codex requires a compatible installed and signed-in Codex application or `codex.exe`.
3. For Claude, run `Setup Claude.cmd` to install the official Agent SDK. Choose “Войти в Claude” in AI settings or run `Login Claude.cmd`; sign in to your own account, then select Claude again.

The DLL is loaded into the running Editor without modifying the game installation. If several Editors are open, run `SC2AILoader.exe --pid <id>`. AI is a separate extension window, not a registered native Editor module.

Chats appear on the left; choose “＋ Новый чат” in the list to start one. The send button changes to stop while a response is running. Codex lists locally available app-server threads; remote or cloud threads may not be accessible. Claude lists local Claude Code sessions, which are distinct from Claude's website chats. Claude Code requires an eligible paid plan or separately billed API access; the free Claude plan alone does not include it ([Anthropic help](https://support.claude.com/en/articles/11145838-use-claude-code-with-your-pro-max-or-ultra-plan)).

The extension detects English and Russian from the Editor menu; other languages fall back to English. User messages and assistant responses are not translated. The conversation distinguishes roles and tool activity with color, and long command entries are shortened in the display.

The assistant receives the open map path and Editor window identity with each message. Abbreviated titles are matched against recent Editor documents and marked as inferred; ambiguous paths are left unresolved. The configured `sc2editor` MCP server is also passed to Claude. Actual map collaboration requires its `live.*` tools and a saved map: MCP uses a separate working copy and opens a checkpoint rather than directly editing the in-memory map. The map name shown in the UI does not by itself confirm an MCP connection. Approval requests are presented; some complex MCP elicitation forms are not yet supported and are declined. Long histories are truncated for display.

See [the implementation notes](docs/chat-integration-plan.ru.md). The Editor version, UI layouts, and scaling settings are not exhaustively tested.

## Author and use

The author is [VoVanRusLvSC2](https://github.com/VoVanRusLvSC2). You may use, copy, modify, and distribute the project, subject to retaining the notice and crediting the author in public mentions or publications. Contact the author on GitHub with questions. See [LICENSE](LICENSE) for the full terms.
