import assert from 'node:assert/strict';
import test from 'node:test';
import { WorkspaceAgentClient, WorkspaceAgentError } from '../src/workspace-agent-client.js';

test('trigger uses Workspace Agent endpoint, beta status header, and safe retry', async () => {
  const calls = [];
  const responses = [
    new Response(JSON.stringify({ error: 'slow down' }), { status: 429 }),
    new Response(JSON.stringify({
      conversation_url: 'https://chatgpt.com/c/test',
      agent_trigger_run_id: 'apirun_test',
    }), { status: 202, headers: { 'Content-Type': 'application/json' } }),
  ];
  const client = new WorkspaceAgentClient({
    triggerId: 'agtch_test',
    accessToken: 'secret-agent-token',
    maxAttempts: 2,
    fetchImpl: async (url, options) => {
      calls.push({ url, options });
      return responses.shift();
    },
    sleep: async () => {},
    random: () => 0,
  });

  const result = await client.trigger({
    input: 'hello',
    conversationKey: 'conversation-1',
    idempotencyKey: 'wrq_test-initial',
  });
  assert.equal(result.runId, 'apirun_test');
  assert.equal(calls.length, 2);
  assert.equal(calls[0].url, 'https://api.chatgpt.com/v1/workspace_agents/agtch_test/trigger');
  assert.equal(calls[0].options.headers.Authorization, 'Bearer secret-agent-token');
  assert.equal(calls[0].options.headers['OpenAI-Beta'], 'workspace_agent_runs=v1');
  assert.equal(calls[0].options.headers['Idempotency-Key'], 'wrq_test-initial');
  assert.deepEqual(JSON.parse(calls[0].options.body), {
    conversation_key: 'conversation-1',
    input: 'hello',
  });
});

test('non-retryable Workspace Agent authorization failure is classified', async () => {
  const client = new WorkspaceAgentClient({
    triggerId: 'agtch_test',
    accessToken: 'bad-token',
    fetchImpl: async () => new Response('{}', { status: 401 }),
    sleep: async () => {},
  });
  await assert.rejects(
    client.getRun('apirun_test'),
    (error) => error instanceof WorkspaceAgentError &&
      error.code === 'AGENT_AUTH' && error.retryable === false,
  );
});
