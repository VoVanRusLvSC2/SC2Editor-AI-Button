// Only queues MCP refresh on this panel's own idle app-server. No process kills.
// Contract: https://developers.openai.com/codex/app-server/ (config/mcpServer/reload).
export function createMcpFreshener(request, fingerprint) {
  let loaded;
  return async function refreshIfChanged() {
    const next = fingerprint();
    if (!next || next === loaded) return false;
    await request('config/mcpServer/reload', null);
    loaded = next;
    return true;
  };
}
