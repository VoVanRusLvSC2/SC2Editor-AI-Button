// Only queues MCP refresh on this panel's own idle app-server. No process kills.
// Contract: https://developers.openai.com/codex/app-server/ (config/mcpServer/reload).
// Request an MCP refresh once per runtime fingerprint. Skip unknown or unchanged files. A
// failed request does not record the fingerprint, allowing a retry.
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
