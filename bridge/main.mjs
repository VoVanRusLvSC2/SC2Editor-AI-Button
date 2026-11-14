import { spawn, execFileSync } from 'node:child_process';
import { createInterface } from 'node:readline';
import {
  existsSync,
  readdirSync,
  statSync,
  readFileSync,
  mkdirSync,
  writeFileSync,
  unlinkSync,
} from 'node:fs';
import { createHash, randomUUID } from 'node:crypto';
import path from 'node:path';
import os from 'node:os';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { createMcpFreshener } from './mcp_refresh.mjs';
import { chatTitle, compactTitle, firstUserTitle, isContextTitle } from './chat_titles.mjs';
import { recordTurnDiff, listChanges, changeDiff, revertChange, workspaceRoot } from './file_changes.mjs';
import { snapshotWorkspace, observedDiff } from './workspace_snapshot.mjs';

// A private stdio channel connects this process only to the editor extension.
// Data fields are base64-encoded UTF-8; stdout never contains debug output.
// C++ bridge protocol: each line contains an event name and tab-separated fields. Fields use
// UTF-8 and Base64 so line breaks do not break framing. Base64 is not encryption.
const b64 = (s) => Buffer.from(String(s ?? ''), 'utf8').toString('base64');
const un64 = (s) => Buffer.from(s || '', 'base64').toString('utf8');
const emit = (kind, ...values) => process.stdout.write([kind, ...values.map(b64)].join('\t') + '\n');
const locale = process.argv[4] || 'enUS';
const russian = locale === 'ru' || locale === 'ruRU';
// Keep the same catalog as the native UI; unknown locales fall back to English.
const translations = JSON.parse(readFileSync(new URL('./locales.json', import.meta.url), 'utf8'));
const tr = (ru, en) => (russian ? ru : (translations[locale]?.[en] ?? en));
const cwd = process.argv[2] && existsSync(process.argv[2]) ? process.argv[2] : process.cwd();
// Current window state: provider, model, chat and running request. generation helps discard
// stale asynchronous results.
let provider = 'codex',
  model = '',
  selected = '',
  running = null,
  generation = 0;
let mapContext = { path: '', dirty: false, caption: '', windowId: '', processId: '', inferred: false },
  mcpStatus = tr('SC2 MCP: проверка...', 'SC2 MCP: checking...');
// Display the MCP status and detected map name. This does not confirm that MCP is bound to the
// map in memory.
function showContext() {
  const name = mapContext.path ? path.basename(mapContext.path) : '';
  emit(
    'mcp',
    mcpStatus +
      ' · ' +
      (name
        ? `${tr('карта', 'map')}: ${name}${mapContext.inferred ? tr(' (по истории редактора)', ' (from editor history)') : ''}${mapContext.dirty ? tr(' (есть несохранённые изменения)', ' (unsaved changes)') : ''}`
        : tr('карта не определена', 'map not identified')),
  );
}
// Add editor context and map instructions to the model prompt. This text does not itself grant
// map access or replace MCP verification.
function mapPrompt(text) {
  const map = mapContext.path
    ? `Document in this SC2 Editor window: ${JSON.stringify(mapContext.path)}. ${mapContext.inferred ? 'The path was inferred from an abbreviated title and Editor history; verify the window identity through MCP. ' : ''}${mapContext.dirty ? 'The Editor contains unsaved changes; the saved file may differ.' : 'This identifies the saved file; inspect the Editor for unsaved changes.'}`
    : `The exact open-map path is unresolved (title: ${JSON.stringify(mapContext.caption)}). Do not guess another map. Read-only connectivity inspection does not require a map path; resolve the explicit path before any edit.`;
  return `[Контекст SC2 Editor]\n${map}\nEditor identity: windowId=${JSON.stringify(mapContext.windowId)}, processId=${JSON.stringify(mapContext.processId)}. Work on this map by default. Default editing workflow: change this current open document, then use live.document.status and live.document.save for a requested same-file save, and summarize the applied changes. Never create, export or open another map/checkpoint/Save As copy unless the user explicitly requests a copy. If a requested in-memory operation (including object placement) is unsupported, report that gap and do not silently substitute checkpoint authoring. A missing save or placement tool may be an old connection: inspect live.capabilities.apiVersion and refresh/reconnect before concluding. Native save includes all current unsaved user edits. The MCP workspace may point elsewhere; do not treat it as the current map. Author map content through MCP. For a connectivity-only test, discover/enable live.windows and live.capabilities or live.memory.capabilities through modkit.tools/modkit.enable_tools in focused profiles, then inspect them read-only. If those discovery helpers are absent, use the tools already exposed by the profile; a missing discovery helper does not establish that live APIs are absent. Do not ask the user to save or call live.connect merely to test connectivity; an unsaved document is not a connection failure. For requested direct-memory Unit scalar edits, discover live.memory.read/set and select the matching Data window from the same process/document, even if this panel belongs to Terrain. These tools accept unsaved document values and do not require live.connect. They currently support existing map-local Unit numeric fields only, not live Terrain, triggers or object creation. If live.memory.* is absent, report the unavailable/stale MCP connection or profile and reconnect it to the updated server; saving will not expose missing tools. live.connect is ONLY the separately selected saved-checkpoint workflow, which imports a private copy and requires a saved file. Use the explicit map path and exact window/process identity; do not infer unsupported capabilities or silently replace either workflow. Preserve unsaved work, user dialogs and uncertain results. Do not claim a memory edit was applied until native value readback confirms it. A title/history path match alone is not authoritative in-memory document binding.\n\n[Сообщение пользователя]\n${text}`;
}
const claudeDirs = new Map();
// Membership belongs to this extension and must not modify Codex desktop state
// or change the working directory of chats operating on the current map.
const editorGroup = 'sc2-ai:in-editor';
// Markers identify chats created in the editor. They do not contain chat history or move map
// files.
const stateDir =
  process.env.SC2_AI_STATE_DIR ||
  path.join(
    process.env.LOCALAPPDATA || path.join(os.homedir(), '.local', 'share'),
    'SC2Editor-AI-Button',
    'in-editor',
  );
const marker = (kind, id) =>
  path.join(
    stateDir,
    createHash('sha256')
      .update(kind + '\0' + id)
      .digest('hex'),
  );
function isEditorChat(kind, id) {
  return !!id && existsSync(marker(kind, id));
}
function markEditorChat(kind, id) {
  mkdirSync(stateDir, { recursive: true });
  try {
    writeFileSync(marker(kind, id), '', { flag: 'wx' });
  } catch (e) {
    if (e.code !== 'EEXIST') throw e;
  }
}
function forgetEditorChat(kind, id) {
  try {
    unlinkSync(marker(kind, id));
  } catch (e) {
    if (e.code !== 'ENOENT') throw e;
  }
}
// Keep provider clients and pending approvals. Pending approvals are declined when the bridge
// closes.
let codex,
  claude,
  aborter,
  nextApproval = 1,
  questionRequest = null,
  loginId = null;
const approvals = new Map();
let sectionPromise = null;
const sectionMoves = new Set();
const editorTitles = new Map(),
  pendingTitles = new Set(),
  titleRetryAt = new Map();
// Try an explicit path, the standard installation and PATH without depending on the author's
// username.
function findCodex() {
  if (process.env.SC2_AI_CODEX_PATH && existsSync(process.env.SC2_AI_CODEX_PATH))
    return process.env.SC2_AI_CODEX_PATH;
  const root = path.join(process.env.LOCALAPPDATA || '', 'OpenAI', 'Codex', 'bin');
  try {
    const candidates = readdirSync(root)
      .map((x) => path.join(root, x, 'codex.exe'))
      .filter(existsSync);
    candidates.sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
    if (candidates.length) return candidates[0];
  } catch {}
  try {
    const values = execFileSync('where.exe', ['codex.exe'], { encoding: 'utf8', windowsHide: true })
      .trim()
      .split(/\r?\n/);
    return values.find(existsSync);
  } catch {
    return null;
  }
}
const codexPath = findCodex();
// Client for the local Codex app-server: JSON requests and responses travel as lines over
// stdin/stdout and are matched by ID.
class AppServer {
  constructor(exe) {
    this.proc = spawn(
      exe,
      [
        ...(process.env.SC2_AI_CODEX_EXTRA_ARGS ? JSON.parse(process.env.SC2_AI_CODEX_EXTRA_ARGS) : []),
        'app-server',
        '--listen',
        'stdio://',
      ],
      { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true },
    );
    this.id = 1;
    this.pending = new Map();
    this.ready = false;
    createInterface({ input: this.proc.stdout }).on('line', (line) => this.onLine(line));
    this.proc.stderr.on('data', () => {});
    this.proc.on('error', (e) =>
      this.failed(tr('Не удалось запустить Codex: ', 'Could not start Codex: ') + e.message),
    );
    this.proc.on('exit', () =>
      this.failed(tr('Codex app-server завершил работу.', 'Codex app-server exited.')),
    );
  }
  refreshMcp = createMcpFreshener((method, params) => this.request(method, params), sc2RuntimeFingerprint);
  failed(message) {
    if (this.closed) return;
    this.ready = false;
    sectionPromise = null;
    for (const p of this.pending.values()) p.reject(Error(message));
    this.pending.clear();
    if (provider === 'codex') emit('error', message);
  }
  send(msg) {
    if (!this.proc.stdin.destroyed) this.proc.stdin.write(JSON.stringify(msg) + '\n');
  }
  request(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = this.id++;
      this.pending.set(id, { resolve, reject });
      this.send({ id, method, params });
      setTimeout(() => {
        if (this.pending.delete(id))
          reject(Error(tr('Нет ответа от Codex: ', 'No response from Codex: ') + method));
      }, 30000).unref();
    });
  }
  // Responses with IDs resolve pending requests. Notifications deliver text deltas, tool
  // events and turn completion.
  onLine(line) {
    let m;
    try {
      m = JSON.parse(line);
    } catch {
      return;
    }
    if (m.id !== undefined && !m.method) {
      const p = this.pending.get(m.id);
      if (!p) return;
      this.pending.delete(m.id);
      m.error ? p.reject(Error(m.error.message || tr('Ошибка Codex', 'Codex error'))) : p.resolve(m.result);
      return;
    }
    if (m.id !== undefined && m.method) {
      this.serverRequest(m);
      return;
    }
    if (m.method === 'account/login/completed' && loginId && m.params?.loginId === loginId) {
      loginId = null;
      if (provider === 'codex') {
        if (m.params.success) codexConnect(generation).catch((e) => emit('error', e.message));
        else emit('error', tr('Вход в Codex не завершён.', 'Codex sign-in did not complete.'));
      }
      return;
    }
    if (provider !== 'codex') return;
    const p = m.params || {};
    if (p.threadId && p.threadId !== selected) return;
    if (
      m.method === 'turn/diff/updated' &&
      running?.provider === 'codex' &&
      (!running.turnId || running.turnId === p.turnId)
    ) {
      running.diff = p.diff;
      running.diffTurnId = p.turnId;
    }
    if (m.method === 'item/agentMessage/delta') emit('delta', p.delta || '');
    if (m.method === 'item/started' && p.item?.type === 'mcpToolCall')
      emit('event', `MCP: ${p.item.server} / ${p.item.tool}`);
    if (m.method === 'item/started' && p.item?.type === 'commandExecution')
      emit('event', tr('Выполняется команда', 'Running a command'));
    if (m.method === 'turn/completed') {
      const completed = running;
      const turn = p.turn || {};
      finishCodexTurn(completed, turn).catch((e) => {
        running = null;
        emit('busy', '0');
        emit('error', e.message);
      });
    }
    if (m.method === 'error' && p.error?.message) emit('error', p.error.message);
  }
  // Forward supported approval and input requests to the window. Explicitly reject unsupported
  // interactive requests.
  serverRequest(m) {
    const supported = ['item/commandExecution/requestApproval', 'item/fileChange/requestApproval'];
    if (supported.includes(m.method)) {
      const id = String(nextApproval++),
        details = JSON.stringify(m.params, null, 2).slice(0, 12000);
      approvals.set(id, (yes) => this.send({ id: m.id, result: { decision: yes ? 'accept' : 'decline' } }));
      emit('approval', id, `${tr('Запрос Codex', 'Codex request')}: ${m.method}\n\n${details}`);
      return;
    }
    if (m.method === 'item/permissions/requestApproval') {
      const id = String(nextApproval++);
      approvals.set(id, (yes) =>
        this.send({
          id: m.id,
          result: { permissions: yes ? m.params.permissions || {} : {}, scope: 'turn' },
        }),
      );
      emit(
        'approval',
        id,
        `${tr('Codex запрашивает дополнительные права:', 'Codex requests additional permissions:')}\n\n${JSON.stringify(m.params, null, 2).slice(0, 12000)}`,
      );
      return;
    }
    if (m.method === 'item/tool/requestUserInput') {
      const questions = m.params?.questions || [];
      if (!questions.length || questions.some((q) => q.isSecret) || questionRequest) {
        this.send({ id: m.id, error: { code: -32601, message: 'This input request is not supported.' } });
        emit(
          'error',
          tr(
            'Секретный или повторный запрос ввода пока не поддерживается.',
            'Secret or repeated input requests are not supported yet.',
          ),
        );
        return;
      }
      questionRequest = { requestId: m.id, questions, index: 0, answers: {} };
      askNext();
      return;
    }
    if (m.method === 'mcpServer/elicitation/request') {
      this.send({ id: m.id, result: { action: 'decline' } });
      emit(
        'error',
        tr(
          'Форма подтверждения MCP пока не поддерживается и была отклонена.',
          'The MCP confirmation form is not supported and was declined.',
        ),
      );
      return;
    }
    // Unsupported interactive requests are rejected explicitly; never auto-approve.
    this.send({
      id: m.id,
      error: { code: -32601, message: 'This client does not support this interactive request.' },
    });
    emit(
      'error',
      `${tr('Этот запрос Codex пока не поддерживается', 'This Codex request is not supported')}: ${m.method}`,
    );
  }
  async start() {
    await this.request('initialize', {
      clientInfo: { name: 'sc2_editor_ai', title: 'SC2 Editor AI', version: '0.8.0' },
      capabilities: {},
    });
    this.send({ method: 'initialized', params: {} });
    this.ready = true;
  }
  close() {
    this.closed = true;
    try {
      this.proc.stdin.end();
      this.proc.kill();
    } catch {}
  }
}
// Find or create the IN_EDITOR section in Codex. It groups chats and does not move the map.
async function ensureEditorSection() {
  if (!sectionPromise) {
    const server = codex;
    sectionPromise = (async () => {
      let cursor = null;
      do {
        const page = await server.request('threadSection/list', { limit: 100, cursor });
        const section = (page.data || []).find((s) => s.name === 'IN_EDITOR');
        if (section?.id) return section.id;
        cursor = page.nextCursor || null;
      } while (cursor);
      const created = await server.request('threadSection/create', { name: 'IN_EDITOR' });
      if (!created.section?.id) throw Error('Missing section ID');
      return created.section.id;
    })().catch((e) => {
      sectionPromise = null;
      throw e;
    });
  }
  return sectionPromise;
}
// Synchronize the section separately from message sending and display synchronization errors
// in the window.
function syncEditorSection(id) {
  if (!id || sectionMoves.has(id)) return;
  sectionMoves.add(id);
  const server = codex;
  ensureEditorSection()
    .then((sectionId) => server.request('thread/section/move', { threadId: id, sectionId }))
    .catch(() => {
      if (server === codex && provider === 'codex')
        emit(
          'event',
          tr(
            'Не удалось синхронизировать чат с разделом IN_EDITOR в Codex; повторим позже',
            'Could not sync the chat to IN_EDITOR in Codex; will retry later',
          ),
        );
    })
    .finally(() => sectionMoves.delete(id));
}
// Display questions from the current provider request one at a time, then return all answers.
function askNext() {
  if (!questionRequest) return;
  const q = questionRequest.questions[questionRequest.index];
  if (!q) {
    codex.send({ id: questionRequest.requestId, result: { answers: questionRequest.answers } });
    questionRequest = null;
    return;
  }
  const choices = (q.options || []).map((o) => `• ${o.label}: ${o.description}`).join('\n');
  emit('question', q.question + (choices ? '\n' + choices : ''));
}
// Read account state, models and sc2editor status. The status alone does not establish access
// to the intended open map.
async function codexConnect(gen) {
  if (!codexPath)
    throw Error(
      tr(
        'Codex не найден. Установите приложение Codex или задайте SC2_AI_CODEX_PATH.',
        'Codex not found. Install Codex or set SC2_AI_CODEX_PATH.',
      ),
    );
  if (!codex || !codex.ready) {
    codex?.close();
    codex = new AppServer(codexPath);
    await codex.start();
  }
  const [account, models, mcp] = await Promise.all([
    codex.request('account/read', { refreshToken: false }),
    codex.request('model/list', { limit: 100 }),
    codex.request('mcpServerStatus/list', { limit: 100 }),
  ]);
  if (gen !== generation || provider !== 'codex') return;
  emit(
    'status',
    account.account ? tr('Codex подключён', 'Codex connected') : tr('Войдите в Codex', 'Sign in to Codex'),
  );
  emit('models_begin');
  for (const m of models.data || [])
    emit('model', m.model || m.id, m.displayName || m.model || m.id, m.isDefault ? '1' : '0');
  emit('models_end');
  const server = (mcp.data || []).find((x) => x.name === 'sc2editor');
  mcpStatus = server
    ? server.toolsError
      ? tr('SC2 MCP: ошибка подключения', 'SC2 MCP: connection error')
      : Object.keys(server.tools || {}).length
        ? tr('SC2 MCP подключён', 'SC2 MCP connected')
        : tr('SC2 MCP настроен', 'SC2 MCP configured')
    : tr('SC2 MCP не найден в настройках Codex', 'SC2 MCP not found in Codex settings');
  showContext();
  await listCodex(gen);
  if (gen === generation && provider === 'codex') emit('ready', '1');
}
// Fetch local app-server chats and associate them with Codex desktop projects and colors.
async function listCodex(gen = generation) {
  // The desktop app stores legacy project names and assignments separately from
  // app-server threads. Read them only for display; never modify desktop state.
  let state = {};
  try {
    state = JSON.parse(
      readFileSync(
        path.join(process.env.CODEX_HOME || path.join(os.homedir(), '.codex'), '.codex-global-state.json'),
        'utf8',
      ),
    );
  } catch {}
  const projects = state['local-projects'] || {},
    appearances = state['project-appearances'] || {},
    assignments = state['thread-project-assignments'] || {},
    projectless = new Set(state['projectless-thread-ids'] || []);
  const migrated = Object.values(state['app-server-project-id-by-legacy-project-id-by-host'] || {}).reduce(
    (all, entry) => Object.assign(all, entry),
    {},
  );
  const findProject = (t) => {
    let id = t.projectId || assignments[t.id]?.projectId;
    if (t.projectId && !projects[id])
      id = Object.keys(migrated).find((key) => migrated[key] === t.projectId) || id;
    if (projectless.has(t.id) && !t.projectId) return null;
    if (!id && t.cwd) {
      const dir = path.resolve(t.cwd).toLowerCase();
      id = Object.keys(projects).find((key) =>
        (projects[key].rootPaths || []).some((root) => {
          const r = path.resolve(root).toLowerCase();
          return dir === r || dir.startsWith(r + path.sep);
        }),
      );
    }
    return projects[id]
      ? {
          id,
          name: projects[id].name || id,
          color: appearances[id]?.color || appearances[t.projectId]?.color || '',
        }
      : null;
  };
  emit('threads_begin');
  let cursor = null,
    count = 0;
  const needsTitle = [];
  do {
    const r = await codex.request('thread/list', { limit: 100, cursor, sortKey: 'updated_at' });
    if (gen !== generation || provider !== 'codex') return;
    for (const t of r.data || []) {
      const marked = isEditorChat('codex', t.id),
        inSection = t.section?.name === 'IN_EDITOR';
      if (marked && !t.section) syncEditorSection(t.id);
      const project =
        marked || inSection ? { id: editorGroup, name: 'IN_EDITOR', color: 'blue' } : findProject(t);
      const hidden = isContextTitle(t.name) || (!t.name && isContextTitle(t.preview));
      const title =
        (hidden && editorTitles.get(t.id)) ||
        chatTitle(
          t.name,
          t.preview,
          hidden ? tr('Чат редактора', 'Editor chat') : tr('Без названия', 'Untitled'),
        );
      if (
        hidden &&
        !editorTitles.has(t.id) &&
        !pendingTitles.has(t.id) &&
        Date.now() >= (titleRetryAt.get(t.id) || 0)
      )
        needsTitle.push(t.id);
      emit(
        'thread',
        t.id,
        title,
        t.status?.type || '',
        project?.id || '',
        project?.name || '',
        project?.color || '',
      );
      count++;
    }
    cursor = r.nextCursor;
  } while (cursor && count < 1000);
  emit('threads_end', cursor ? tr('Есть дополнительные чаты', 'More chats are available') : '');
  // Resolve hidden-context titles in the background without blocking the chat list.
  if (needsTitle.length) void resolveEditorTitles(needsTitle, gen);
}
// Resolve placeholder titles asynchronously and ignore results from an older provider generation.
async function resolveEditorTitles(ids, gen) {
  const server = codex,
    queue = ids.slice(0, 50);
  async function worker() {
    while (queue.length && gen === generation && provider === 'codex' && server === codex) {
      const id = queue.shift();
      if (pendingTitles.has(id)) continue;
      pendingTitles.add(id);
      try {
        const result = await server.request('thread/turns/list', {
          threadId: id,
          limit: 1,
          itemsView: 'full',
          sortDirection: 'asc',
        });
        const title = firstUserTitle(result.data);
        if (title) {
          editorTitles.set(id, title);
          titleRetryAt.delete(id);
          if (provider === 'codex' && server === codex) emit('thread_title', id, title);
        } else titleRetryAt.set(id, Date.now() + 60000);
      } catch {
        titleRetryAt.set(id, Date.now() + 60000);
      } finally {
        pendingTitles.delete(id);
      }
    }
  }
  await Promise.all(Array.from({ length: Math.min(3, queue.length) }, () => worker()));
}
function statusText(status) {
  const words = {
    completed: tr('выполнено', 'completed'),
    failed: tr('ошибка', 'failed'),
    inProgress: tr('выполняется', 'running'),
    interrupted: tr('остановлено', 'stopped'),
    declined: tr('отклонено', 'declined'),
  };
  return words[status] || status || tr('неизвестно', 'unknown');
}
// The model receives editor context, but the displayed history shows the original user
// message.
function unwrapUserMessage(raw) {
  const marker = '[Сообщение пользователя]\n';
  return raw.startsWith('[Контекст SC2 Editor]\n') && raw.includes(marker)
    ? raw.slice(raw.indexOf(marker) + marker.length)
    : raw;
}
function userMessage(item) {
  const content = item.content || [];
  const text = unwrapUserMessage(content.map((c) => c.text || '').join(''));
  const images = content.filter((c) => c.type === 'localImage' || c.type === 'image').length;
  return text + (images ? '\n[' + tr('Изображение', 'Image') + (images > 1 ? ' × ' + images : '') + ']' : '');
}
function itemText(item) {
  if (item.type === 'userMessage') return tr('Вы: ', 'You: ') + userMessage(item);
  if (item.type === 'agentMessage') return 'AI: ' + (item.text || '');
  if (item.type === 'mcpToolCall') return `[MCP: ${item.server} / ${item.tool} — ${statusText(item.status)}]`;
  if (item.type === 'commandExecution') {
    const command = (item.command || '').replace(/\s+/g, ' ').trim();
    const short = command.length > 180 ? command.slice(0, 177) + '…' : command;
    return `[${tr('Команда', 'Command')} — ${statusText(item.status)}${short ? ': ' + short : ''}]`;
  }
  if (item.type === 'fileChange')
    return `[${tr('Изменения файлов', 'File changes')} — ${statusText(item.status)}]`;
  return '';
}
// Load recent chat turns. Truncating displayed text does not delete the provider's remaining
// history.
async function openCodex(id, gen) {
  const result = await codex.request('thread/turns/list', {
    threadId: id,
    limit: 100,
    itemsView: 'full',
    sortDirection: 'desc',
  });
  if (gen !== generation) return;
  const lines = [];
  for (const turn of [...(result.data || [])].reverse())
    for (const item of turn.items || []) {
      let text = itemText(item);
      if (text) lines.push(text);
    }
  let history = lines.join('\r\n\r\n');
  if (history.length > 200000)
    history =
      tr('[Показана последняя часть истории]\r\n', '[Showing the latest part of the history]\r\n') +
      history.slice(-200000);
  emit('history', history);
  if (result.nextCursor)
    emit('event', tr('Показаны последние 100 сообщений диалога.', 'Showing the last 100 messages.'));
}
// Check the PNG path, size and signature again before sending, in addition to the C++ UI
// checks.
function readAttachedImage(file) {
  if (!file) return null;
  if (!path.isAbsolute(file)) throw Error(tr('Некорректный путь изображения.', 'Invalid image path.'));
  const stat = statSync(file);
  if (!stat.isFile() || stat.size > 7 * 1024 * 1024 || stat.size < 8)
    throw Error(tr('Изображение недоступно или слишком большое.', 'The image is unavailable or too large.'));
  const bytes = readFileSync(file);
  if (!bytes.subarray(0, 8).equals(Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])))
    throw Error(tr('Некорректный формат изображения.', 'Invalid image format.'));
  return bytes;
}
function loadAttachedImages(paths) {
  if (paths.length > 5)
    throw Error(tr('Можно прикрепить не более 5 изображений.', 'You can attach up to 5 images.'));
  const images = paths.map((path) => ({ path, bytes: readAttachedImage(path) }));
  if (images.reduce((total, image) => total + image.bytes.length, 0) > 20 * 1024 * 1024)
    throw Error(tr('Общий размер изображений превышает 20 МБ.', 'The combined image size exceeds 20 MB.'));
  return images;
}
function displayUserText(text, count) {
  return (
    tr('Вы: ', 'You: ') +
    text +
    (count ? '\n[' + tr('Изображение', 'Image') + (count > 1 ? ' × ' + count : '') + ']' : '')
  );
}
// The first message creates a chat; subsequent messages resume it. Derive the working
// directory from the map and attach current context to every request.
async function sendCodex(text, images) {
  running = { provider: 'codex', turnId: null, threadId: selected, stopRequested: false };
  if (await codex.refreshMcp())
    emit(
      'event',
      tr(
        'Запрошено обновление MCP перед ответом; возможности проверяются инструментами',
        'MCP refresh queued before the turn; inspect tools for actual capabilities',
      ),
    );
  const turnCwd = mapContext.path ? path.dirname(mapContext.path) : cwd;
  running.cwd = turnCwd;
  try {
    running.root = workspaceRoot(turnCwd);
    running.snapshot = await snapshotWorkspace(turnCwd);
  } catch {}
  let createdHere = false;
  if (!selected) {
    const r = await codex.request('thread/start', {
      cwd: turnCwd,
      model: model || null,
      approvalPolicy: 'on-request',
      approvalsReviewer: 'user',
      sandbox: 'workspace-write',
      ...(process.env.SC2_AI_TEST_EPHEMERAL === '1' ? { ephemeral: true } : {}),
    });
    selected = r.thread.id;
    const title = compactTitle(text);
    if (title) editorTitles.set(selected, title);
    if (process.env.SC2_AI_TEST_EPHEMERAL !== '1') {
      markEditorChat('codex', selected);
      createdHere = true;
    }
    emit('selected', selected);
    await listCodex();
  } else
    await codex.request('thread/resume', {
      threadId: selected,
      excludeTurns: true,
      cwd: turnCwd,
      sandbox: 'workspace-write',
      approvalPolicy: 'on-request',
      approvalsReviewer: 'user',
    });
  emit('line', displayUserText(text, images.length));
  emit('line', 'AI: ');
  emit('busy', '1');
  const input = [
    {
      type: 'text',
      text: mapPrompt(text || tr('Опиши прикреплённые изображения.', 'Describe the attached images.')),
      text_elements: [],
    },
    ...images.map((image) => ({ type: 'localImage', path: image.path })),
  ];
  const r = await codex.request('turn/start', {
    threadId: selected,
    cwd: turnCwd,
    input,
    model: model || null,
    sandboxPolicy: { type: 'workspaceWrite', writableRoots: [turnCwd], networkAccess: false },
  });
  emit('accepted');
  if (createdHere) syncEditorSection(selected);
  if (running?.provider === 'codex') {
    running.turnId = r.turn.id;
    running.threadId = selected;
    if (running.stopRequested)
      await codex.request('turn/interrupt', { threadId: selected, turnId: r.turn.id });
  }
}
// Fingerprint MCP runtime files to detect changes to the installed version. A file change does
// not establish availability of any particular tool.
let sc2RuntimeFiles;
function sc2RuntimeFingerprint() {
  if (!sc2RuntimeFiles) {
    let config = sc2Config()?.sc2editor;
    if (config?.env?.SC2_AI_MCP_SPEC) {
      try {
        config = JSON.parse(Buffer.from(config.env.SC2_AI_MCP_SPEC, 'base64').toString('utf8'));
      } catch {
        return null;
      }
    }
    const entry = config?.args?.find(
      (arg) => typeof arg === 'string' && path.isAbsolute(arg) && arg.endsWith('.js') && existsSync(arg),
    );
    if (!entry) return null;
    const base = path.dirname(entry);
    sc2RuntimeFiles = [
      entry,
      path.join(base, 'app/moduleSelection.js'),
      ...[
        'tools',
        'workspace',
        'document',
        'memory',
        'memoryTools',
        'memoryDesktop',
        'desktop',
        'editorLease',
      ].map((name) => path.join(base, 'modules/live', name + '.js')),
      ...['sc2-mcp-live.exe', 'sc2-mcp-live-memory.json'].map((name) =>
        path.resolve(base, '../native/bin/windows-x64', name),
      ),
    ];
  }
  const hash = createHash('sha256');
  for (const file of sc2RuntimeFiles) {
    hash.update(file);
    try {
      hash.update(readFileSync(file));
    } catch {
      hash.update('missing');
    }
  }
  return hash.digest('hex');
}
// Read sc2editor configuration from Codex for Claude and the MCP version check. Support stdio
// and HTTP, with a wrapper when a cwd is specified.
function sc2Config() {
  if (!codexPath) return undefined;
  try {
    const config = JSON.parse(
      execFileSync(codexPath, ['mcp', 'get', 'sc2editor', '--json'], {
        encoding: 'utf8',
        windowsHide: true,
        timeout: 8000,
        stdio: ['ignore', 'pipe', 'ignore'],
      }),
    );
    const t = config.transport;
    if (!config.enabled || !t) return undefined;
    if (t.type === 'stdio') {
      const env = { ...process.env, ...(t.env || {}) };
      for (const n of t.env_vars || []) if (process.env[n] !== undefined) env[n] = process.env[n];
      if (t.cwd) {
        const wrapper = path.join(path.dirname(fileURLToPath(import.meta.url)), 'mcp_spawn.mjs');
        env.SC2_AI_MCP_SPEC = Buffer.from(
          JSON.stringify({ command: t.command, args: t.args || [], cwd: t.cwd }),
        ).toString('base64');
        return { sc2editor: { type: 'stdio', command: process.execPath, args: [wrapper], env } };
      }
      return { sc2editor: { type: 'stdio', command: t.command, args: t.args || [], env } };
    }
    if ((t.type === 'http' || t.type === 'streamable_http') && t.url)
      return { sc2editor: { type: 'http', url: t.url, headers: t.http_headers || {} } };
  } catch {}
  return undefined;
}
// Connect the Claude Agent SDK. This implementation requires ANTHROPIC_API_KEY; signing in to
// the Claude website alone is insufficient.
async function claudeConnect(gen) {
  try {
    claude = await import(
      process.env.SC2_AI_CLAUDE_MODULE
        ? pathToFileURL(process.env.SC2_AI_CLAUDE_MODULE).href
        : '@anthropic-ai/claude-agent-sdk'
    );
  } catch {
    throw Error(
      tr(
        'Claude Agent SDK не установлен. Выполните npm ci в папке bridge.',
        'Claude Agent SDK is not installed. Run npm ci in the bridge folder.',
      ),
    );
  }
  const loggedIn = !!process.env.ANTHROPIC_API_KEY;
  if (gen !== generation || provider !== 'claude') return;
  emit(
    'status',
    loggedIn
      ? tr('Claude: API настроен', 'Claude API configured')
      : tr('Claude: нужен API-ключ', 'Claude: API key required'),
  );
  emit('models_begin');
  for (const [id, name] of [
    ['', tr('По умолчанию', 'Default')],
    ['sonnet', 'Sonnet'],
    ['opus', 'Opus'],
    ['haiku', 'Haiku'],
  ])
    emit('model', id, name, id === '' ? '1' : '0');
  emit('models_end');
  mcpStatus = sc2Config()
    ? tr('SC2 MCP настроен для Claude', 'SC2 MCP configured for Claude')
    : tr('SC2 MCP не найден в настройках Codex', 'SC2 MCP not found in Codex settings');
  showContext();
  await listClaude(gen);
  if (gen === generation && provider === 'claude') emit('ready', '1');
}
// List local Claude sessions and group them by working directory; mark editor-created sessions
// as IN_EDITOR.
async function listClaude(gen = generation) {
  emit('threads_begin');
  const sessions = await claude.listSessions({ limit: 200 });
  if (gen !== generation || provider !== 'claude') return;
  for (const s of sessions) {
    if (s.cwd) claudeDirs.set(s.sessionId, s.cwd);
    const own = isEditorChat('claude', s.sessionId);
    emit(
      'thread',
      s.sessionId,
      chatTitle(s.customTitle, s.summary || s.firstPrompt, tr('Чат редактора', 'Editor chat')),
      '',
      own ? editorGroup : s.cwd || '',
      own ? 'IN_EDITOR' : s.cwd ? path.basename(s.cwd) : '',
      own ? 'blue' : '',
    );
  }
  emit('threads_end');
}
// Read an existing Claude session for display while retaining its provider session ID.
async function openClaude(id, gen) {
  const msgs = await claude.getSessionMessages(id, { limit: 200 });
  if (gen !== generation) return;
  const lines = [];
  for (const m of msgs) {
    const parts =
      typeof m.message?.content === 'string' ? [{ text: m.message.content }] : m.message?.content || [];
    const text = parts
      .filter((x) => x.type === 'text' || typeof x.text === 'string')
      .map((x) => x.text)
      .join('\n');
    const images = parts.filter((x) => x.type === 'image').length;
    if (text || images)
      lines.push(
        (m.type === 'user' ? tr('Вы: ', 'You: ') : 'AI: ') +
          (m.type === 'user' ? unwrapUserMessage(text) : text) +
          (images ? '\n[' + tr('Изображение', 'Image') + (images > 1 ? ' × ' + images : '') + ']' : ''),
      );
  }
  let history = lines.join('\r\n\r\n');
  if (history.length > 200000)
    history =
      tr('[Показана последняя часть истории]\r\n', '[Showing the latest part of the history]\r\n') +
      history.slice(-200000);
  emit('history', history);
}
// Send map context and images to Claude, show tool permission requests, and use
// AbortController to stop a response.
async function sendClaude(text, images) {
  if (!process.env.ANTHROPIC_API_KEY)
    throw Error(
      tr(
        'Для Claude в этом приложении нужен ANTHROPIC_API_KEY из Claude Console. Вход по подписке Claude здесь не поддерживается.',
        'Claude in this application requires ANTHROPIC_API_KEY from Claude Console. Claude subscription sign-in is not supported here.',
      ),
    );
  const current = selected;
  aborter = new AbortController();
  emit('line', displayUserText(text, images.length));
  emit('line', 'AI: ');
  emit('busy', '1');
  running = { provider: 'claude' };
  let streamed = false,
    failed = false;
  const options = {
    cwd: (current && claudeDirs.get(current)) || (mapContext.path ? path.dirname(mapContext.path) : cwd),
    abortController: aborter,
    includePartialMessages: true,
    permissionMode: 'default',
    settingSources: ['user', 'project'],
    ...(current ? { resume: current } : {}),
    ...(model ? { model } : {}),
    ...(sc2Config() ? { mcpServers: sc2Config() } : {}),
    canUseTool: async (toolName, input) => {
      const id = String(nextApproval++);
      return await new Promise((resolve) => {
        approvals.set(id, (yes) =>
          resolve(
            yes
              ? { behavior: 'allow', updatedInput: input }
              : { behavior: 'deny', message: tr('Отклонено пользователем', 'Declined by user') },
          ),
        );
        emit(
          'approval',
          id,
          `${tr('Запрос Claude', 'Claude request')}: ${toolName}\n\n${JSON.stringify(input, null, 2).slice(0, 12000)}`,
        );
      });
    },
  };
  let before, root;
  try {
    root = workspaceRoot(options.cwd);
    before = await snapshotWorkspace(options.cwd);
  } catch {}
  const changeTurn = randomUUID();
  const content = mapPrompt(text || tr('Опиши прикреплённые изображения.', 'Describe the attached images.'));
  async function* imagePrompt() {
    yield {
      type: 'user',
      message: {
        role: 'user',
        content: [
          ...images.map((image) => ({
            type: 'image',
            source: { type: 'base64', media_type: 'image/png', data: image.bytes.toString('base64') },
          })),
          { type: 'text', text: content },
        ],
      },
      parent_tool_use_id: null,
    };
  }
  let accepted = false;
  try {
    for await (const message of claude.query({ prompt: images.length ? imagePrompt() : content, options })) {
      if (!accepted) {
        accepted = true;
        emit('accepted');
      }
      if (message.type === 'system' && message.subtype === 'init' && message.session_id && !selected) {
        selected = message.session_id;
        markEditorChat('claude', selected);
        emit('selected', selected);
      }
      if (
        message.type === 'stream_event' &&
        message.event?.type === 'content_block_delta' &&
        message.event.delta?.type === 'text_delta'
      ) {
        emit('delta', message.event.delta.text);
        streamed = true;
      }
      if (message.type === 'assistant' && !streamed) {
        for (const part of message.message?.content || []) if (part.type === 'text') emit('delta', part.text);
      }
      if (message.type === 'result' && message.is_error) {
        failed = true;
        emit(
          'error',
          (message.errors || []).join('\n') ||
            tr('Claude завершился с ошибкой.', 'Claude returned an error.'),
        );
      }
    }
    if (!failed) emit('status', tr('Готово', 'Ready'));
    await listClaude();
  } finally {
    try {
      if (before && root && selected) {
        const after = await snapshotWorkspace(options.cwd),
          diff = observedDiff(root, before, after);
        if (diff) {
          if (recordTurnDiff(stateDir, changeThread(), changeTurn, options.cwd, diff).length) emitChanges();
          else
            emit(
              'event',
              tr(
                'Не удалось обработать изменения файлов. Проверьте установку Git.',
                'Could not process the file changes. Check that Git is installed.',
              ),
            );
        }
      }
    } catch {
      emit(
        'event',
        tr('Не удалось сохранить список изменений файлов', 'Could not save the file changes list'),
      );
    }
    running = null;
    aborter = null;
    emit('busy', '0');
  }
}
// Clear the selection and advance generation when switching providers so old asynchronous
// results cannot overwrite the new state.
async function connect(which) {
  const gen = ++generation;
  provider = which;
  selected = '';
  model = '';
  emit('ready', '0');
  emit('clear');
  emit('changes_begin');
  emit('changes_end');
  emit('threads_begin');
  emit('threads_end');
  emit('status', tr('Подключение...', 'Connecting...'));
  emit('provider', which);
  try {
    if (which === 'codex') await codexConnect(gen);
    else await claudeConnect(gen);
  } catch (e) {
    if (gen === generation) throw e;
  }
}
// Shell commands do not always emit structured file changes. Compare a bounded set of text
// files from before/after the turn as a fallback; no other files or open map memory are tracked.
async function finishCodexTurn(completed, turn) {
  if (completed?.provider === 'codex' && completed.cwd && turn.id) {
    try {
      let diff = completed.diffTurnId === turn.id ? completed.diff || '' : '';
      if (completed.snapshot && completed.root) {
        const after = await snapshotWorkspace(completed.cwd);
        diff += observedDiff(completed.root, completed.snapshot, after);
      }
      if (diff) {
        if (recordTurnDiff(stateDir, changeThread(), turn.id, completed.cwd, diff).length) emitChanges();
        else
          emit(
            'event',
            tr(
              'Не удалось обработать изменения файлов. Проверьте установку Git.',
              'Could not process the file changes. Check that Git is installed.',
            ),
          );
      }
    } catch {
      emit(
        'event',
        tr('Не удалось сохранить список изменений файлов', 'Could not save the file changes list'),
      );
    }
  }
  running = null;
  emit('busy', '0');
  emit(
    'status',
    turn.status === 'completed'
      ? tr('Готово', 'Ready')
      : `${tr('Завершено', 'Finished')}: ${statusText(turn.status)}`,
  );
  if (turn.error?.message) emit('error', turn.error.message);
  listCodex(generation).catch((e) => emit('error', e.message));
}
// Only supported text-file changes have locally recorded undo information.

function changeThread() {
  return provider + ':' + selected;
}
// Send an explicit begin/end pair so the native window can replace its file-change list.
function emitChanges() {
  emit('changes_begin');
  if (selected)
    for (const item of listChanges(stateDir, changeThread()))
      emit('change', item.id, item.path, String(item.added), String(item.removed), item.reverted ? '1' : '0');
  emit('changes_end');
}
function changesError(error) {
  if (/changed after this response/.test(error.message))
    return tr(
      'Файл изменился после ответа. Откат отменён.',
      'The file changed after this response. Undo was cancelled.',
    );
  return tr('Не удалось безопасно откатить изменение файла.', 'Could not safely undo the file change.');
}
// Dispatch C++ window commands for providers, chats, models, sign-in, messages, stopping and
// interactive answers.
async function handle(line) {
  const [cmd, ...args] = line.split('\t');
  const a = args.map(un64);
  if (cmd === 'provider') {
    if (running)
      throw Error(
        tr('Остановите текущий ответ перед переключением.', 'Stop the current response before switching.'),
      );
    await connect(a[0] === 'claude' ? 'claude' : 'codex');
  } else if (cmd === 'login') {
    if (provider !== 'codex' || !codex?.ready)
      throw Error(tr('Сначала подключите Codex.', 'Connect Codex first.'));
    if (running) throw Error(tr('Дождитесь завершения ответа.', 'Wait for the response to finish.'));
    if (loginId) {
      const previous = loginId;
      loginId = null;
      await codex.request('account/login/cancel', { loginId: previous });
    }
    const result = await codex.request('account/login/start', { type: 'chatgpt' });
    let url;
    try {
      url = new URL(result.authUrl);
    } catch {
      throw Error(tr('Codex вернул неверную ссылку входа.', 'Codex returned an invalid sign-in URL.'));
    }
    if (
      url.protocol !== 'https:' ||
      !['auth.openai.com', 'chatgpt.com'].includes(url.hostname) ||
      url.username ||
      url.password
    )
      throw Error(
        tr('Codex вернул неподдерживаемый адрес входа.', 'Codex returned an unsupported sign-in address.'),
      );
    if (!result.loginId) throw Error(tr('Не удалось начать вход в Codex.', 'Could not start Codex sign-in.'));
    loginId = result.loginId;
    emit('login_url', url.href);
    emit('status', tr('Завершите вход в браузере', 'Finish sign-in in your browser'));
  } else if (cmd === 'map') {
    mapContext = {
      path: a[0] || '',
      dirty: a[1] === '1',
      caption: a[2] || '',
      windowId: a[3] || '',
      processId: a[4] || '',
      inferred: a[5] === '1',
    };
    showContext();
  } else if (cmd === 'refresh') {
    if (provider === 'codex') await listCodex();
    else await listClaude();
  } else if (cmd === 'delete') {
    if (running)
      throw Error(tr('Остановите ответ перед удалением чата.', 'Stop the response before deleting a chat.'));
    const id = a[0];
    if (!id || id !== selected) throw Error(tr('Выберите чат для удаления.', 'Select a chat to delete.'));
    const deletingProvider = provider,
      gen = ++generation;
    running = { provider: 'deleting' };
    emit('busy', '1');
    try {
      if (deletingProvider === 'codex') await codex.request('thread/delete', { threadId: id });
      else {
        if (typeof claude.deleteSession !== 'function')
          throw Error(
            tr('Обновите Claude Agent SDK для удаления чатов.', 'Update Claude Agent SDK to delete chats.'),
          );
        await claude.deleteSession(id, claudeDirs.get(id) ? { dir: claudeDirs.get(id) } : {});
        claudeDirs.delete(id);
      }
      editorTitles.delete(id);
      titleRetryAt.delete(id);
      try {
        forgetEditorChat(deletingProvider, id);
      } catch {
        emit(
          'event',
          tr('Не удалось очистить метку удалённого чата', 'Could not remove the deleted chat marker'),
        );
      }
      if (gen === generation && provider === deletingProvider && selected === id) {
        selected = '';
        emit('clear');
        emit('selected', '');
        deletingProvider === 'codex' ? await listCodex(gen) : await listClaude(gen);
      }
    } finally {
      running = null;
      emit('busy', '0');
    }
  } else if (cmd === 'open') {
    if (running)
      throw Error(tr('Остановите ответ перед выбором чата.', 'Stop the response before selecting a chat.'));
    selected = a[0];
    let g = ++generation;
    emit('busy', '1');
    try {
      provider === 'codex' ? await openCodex(selected, g) : await openClaude(selected, g);
    } finally {
      if (g === generation) emit('busy', '0');
    }
    if (g === generation) {
      emit('selected', selected);
      emitChanges();
    }
  } else if (cmd === 'new') {
    if (running) throw Error(tr('Остановите текущий ответ.', 'Stop the current response first.'));
    selected = '';
    generation++;
    emit('clear');
    emit('selected', '');
    emitChanges();
  } else if (cmd === 'change_view') {
    if (!selected) throw Error(tr('Выберите чат.', 'Select a chat.'));
    const diff = changeDiff(stateDir, changeThread(), a[0]);
    emit('change_diff', selected, a[0], diff.slice(0, 200000));
  } else if (cmd === 'change_undo') {
    if (running) throw Error(tr('Дождитесь завершения ответа.', 'Wait for the response to finish.'));
    if (!selected) throw Error(tr('Выберите чат.', 'Select a chat.'));
    try {
      const file = revertChange(stateDir, changeThread(), a[0]);
      emit('event', tr('Изменение отменено: ', 'Change undone: ') + file);
      emitChanges();
    } catch (error) {
      emit('error', changesError(error));
    }
  } else if (cmd === 'model') model = a[0];
  else if (cmd === 'answer' && questionRequest) {
    const q = questionRequest.questions[questionRequest.index++];
    questionRequest.answers[q.id] = { answers: [a[0] || ''] };
    askNext();
  } else if (cmd === 'send') {
    if (running) throw Error(tr('Дождитесь завершения ответа.', 'Wait for the response to finish.'));
    const images = loadAttachedImages(a.slice(1).filter(Boolean));
    if (!a[0]?.trim() && !images.length) return;
    provider === 'codex' ? await sendCodex(a[0] || '', images) : await sendClaude(a[0] || '', images);
  } else if (cmd === 'stop') {
    if (questionRequest) {
      codex.send({
        id: questionRequest.requestId,
        error: { code: -32800, message: 'Input cancelled by user.' },
      });
      questionRequest = null;
    }
    if (running?.provider === 'codex') {
      if (running.turnId)
        await codex.request('turn/interrupt', { threadId: running.threadId, turnId: running.turnId });
      else running.stopRequested = true;
    } else aborter?.abort();
  } else if (cmd === 'approve') {
    const f = approvals.get(a[0]);
    if (f) {
      approvals.delete(a[0]);
      f(a[1] === 'yes');
    }
  }
}
// Read commands until the pipe closes. Report an already-active chat separately and decline
// pending approvals on exit.
createInterface({ input: process.stdin })
  .on('line', (line) => {
    handle(line).catch((e) => {
      if (!line.startsWith('change_')) {
        running = null;
        emit('busy', '0');
      }
      const message = e.message || String(e);
      emit(
        'error',
        /already has an active writer/i.test(message)
          ? tr(
              'Этот чат занят другим процессом Codex. Продолжите его там или освободите чат и повторите. Текст и вложения сохранены.',
              'This chat is in use by another Codex process. Continue it there or release it and try again. Your text and attachments are preserved.',
            )
          : message,
      );
    });
  })
  .on('close', () => {
    for (const f of approvals.values()) f(false);
    aborter?.abort();
    codex?.close();
    process.exit(0);
  });
process.on('uncaughtException', (e) => emit('error', e.message || String(e)));
connect(process.argv[3] === 'claude' ? 'claude' : 'codex').catch((e) =>
  emit('error', e.message || String(e)),
);
