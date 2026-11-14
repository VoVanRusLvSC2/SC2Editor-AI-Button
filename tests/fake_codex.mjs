// Provide a deterministic Codex app-server substitute for bridge integration tests.
import { createInterface } from 'node:readline';
import path from 'node:path';
import { writeFileSync } from 'node:fs';
const reply = (id, result) => process.stdout.write(JSON.stringify({ id, result }) + '\n');
const event = (method, params) => process.stdout.write(JSON.stringify({ method, params }) + '\n');
let pendingApproval = false,
  resumes = 0,
  deleted = false,
  sectionCreated = false,
  sectionMoved = false;
createInterface({ input: process.stdin }).on('line', (line) => {
  const m = JSON.parse(line);
  if (m.id === 901 && m.result) {
    if (m.result.answers.q1.answers[0] !== 'вариант') process.exit(8);
    process.stdout.write(
      JSON.stringify({
        id: 900,
        method: 'item/commandExecution/requestApproval',
        params: { threadId: 'test-1', command: 'example' },
      }) + '\n',
    );
    return;
  }
  if (m.id === 900 && m.result) {
    if (m.result.decision !== 'decline') process.exit(7);
    event('item/agentMessage/delta', { threadId: 'test-1', delta: 'ответ' });
    event('turn/completed', { threadId: 'test-1', turn: { status: 'completed' } });
    return;
  }
  if (m.id === undefined) return;
  switch (m.method) {
    case 'initialize':
      reply(m.id, {});
      break;
    case 'account/read':
      reply(m.id, { account: { type: 'chatgpt' } });
      break;
    case 'account/login/start':
      reply(m.id, { type: 'chatgpt', loginId: 'login-1', authUrl: 'https://chatgpt.com/auth/test' });
      setTimeout(() => event('account/login/completed', { loginId: 'login-1', success: true }), 20);
      break;
    case 'model/list':
      reply(m.id, { data: [{ model: 'test-model', displayName: 'Test', isDefault: true }] });
      break;
    case 'mcpServerStatus/list':
      reply(m.id, { data: [{ name: 'sc2editor', tools: { one: {} } }] });
      break;
    case 'thread/list':
      reply(m.id, {
        data: deleted
          ? []
          : [
              {
                id: 'test-1',
                name: process.env.SC2_TEST_CONTEXT_TITLE
                  ? '[Контекст SC2 Editor] Document in this SC2 Editor window: hidden'
                  : 'Тест',
                projectId: 'project-test',
                status: { type: 'idle' },
                ...(sectionMoved ? { section: { id: 'editor-section', name: 'IN_EDITOR' } } : {}),
              },
            ],
      });
      break;
    case 'threadSection/list':
      reply(m.id, {
        data: sectionCreated ? [{ id: 'editor-section', name: 'IN_EDITOR' }] : [],
        nextCursor: null,
      });
      break;
    case 'threadSection/create':
      if (m.params.name !== 'IN_EDITOR') process.exit(15);
      sectionCreated = true;
      reply(m.id, { section: { id: 'editor-section', name: 'IN_EDITOR' } });
      break;
    case 'thread/section/move':
      if (m.params.threadId !== 'test-1' || m.params.sectionId !== 'editor-section') process.exit(16);
      sectionMoved = true;
      reply(m.id, {});
      break;
    case 'thread/turns/list':
      reply(m.id, {
        data: [
          {
            items: [
              {
                type: 'userMessage',
                content: [
                  { text: '[Контекст SC2 Editor]\nINTERNAL MAP CONTEXT\n\n[Сообщение пользователя]\nвопрос' },
                ],
              },
              { type: 'agentMessage', text: 'история' },
              { type: 'commandExecution', command: 'example ' + 'x'.repeat(400), status: 'completed' },
            ],
          },
        ],
      });
      break;
    case 'thread/resume':
      if (m.params.sandbox !== 'workspace-write' || !m.params.cwd) process.exit(11);
      if (process.env.SC2_TEST_COLLISION_ONCE === '1' && ++resumes === 1) {
        process.stdout.write(
          JSON.stringify({
            id: m.id,
            error: { code: -32000, message: 'thread test-1 already has an active writer' },
          }) + '\n',
        );
        break;
      }
      reply(m.id, { thread: { id: 'test-1' } });
      break;
    case 'thread/start':
      reply(m.id, { thread: { id: 'test-1' } });
      break;
    case 'thread/delete':
      if (m.params.threadId !== 'test-1' || !sectionMoved) process.exit(14);
      deleted = true;
      reply(m.id, {});
      break;
    case 'turn/start': {
      if (process.env.SC2_TEST_CHANGE_DIR) {
        const target = path.join(process.env.SC2_TEST_CHANGE_DIR, 'changes.txt');
        writeFileSync(target, 'after\n');
        reply(m.id, { turn: { id: 'turn-change' } });
        const diff =
          'diff --git a/changes.txt b/changes.txt\n--- a/changes.txt\n+++ b/changes.txt\n@@ -1 +1 @@\n-before\n+after\n';
        setTimeout(() => {
          if (!process.env.SC2_TEST_NO_DIFF)
            event('turn/diff/updated', { threadId: 'test-1', turnId: 'turn-change', diff });
          event('turn/completed', { threadId: 'test-1', turn: { id: 'turn-change', status: 'completed' } });
        }, 20);
        break;
      }
      if (
        m.params.sandboxPolicy?.type !== 'workspaceWrite' ||
        !m.params.sandboxPolicy.writableRoots.includes(m.params.cwd)
      )
        process.exit(12);
      const hasImage = m.params.input[0].text.includes('другая карта'),
        expected = hasImage ? process.env.SC2_TEST_MAP2 : process.env.SC2_TEST_MAP;
      if (
        m.params.cwd !== path.dirname(expected) ||
        !m.params.input[0].text.includes(JSON.stringify(expected)) ||
        !m.params.input[0].text.includes('windowId="123"')
      )
        process.exit(9);
      if (
        hasImage &&
        (m.params.input[1]?.type !== 'localImage' ||
          m.params.input[1]?.path !== process.env.SC2_TEST_IMAGE ||
          m.params.input[2]?.type !== 'localImage' ||
          m.params.input[2]?.path !== process.env.SC2_TEST_IMAGE2 ||
          m.params.input.length !== 3)
      )
        process.exit(10);
      reply(m.id, { turn: { id: 'turn-1' } });
      pendingApproval = true;
      setTimeout(() => {
        if (pendingApproval)
          process.stdout.write(
            JSON.stringify({
              id: 901,
              method: 'item/tool/requestUserInput',
              params: { threadId: 'test-1', questions: [{ id: 'q1', question: 'Выбор?', options: [] }] },
            }) + '\n',
          );
      }, 10);
      break;
    }
    case 'turn/interrupt':
      reply(m.id, {});
      break;
    default:
      reply(m.id, {});
  }
});
