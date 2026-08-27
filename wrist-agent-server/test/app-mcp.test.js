import assert from 'node:assert/strict';
import { mkdtemp, readFile, readdir, rm } from 'node:fs/promises';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { once } from 'node:events';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import { createApp } from '../src/app.js';
import { RequestStore } from '../src/store.js';

const DEVICE_TOKEN = 'device-token-that-is-long-and-random-123456';
const PEPPER = 'callback-pepper-that-is-long-and-independent-123456';

async function fixture(options = {}) {
  const directory = await mkdtemp(path.join(tmpdir(), 'wrist-agent-test-'));
  const triggerCalls = [];
  const runStatuses = new Map();
  const agentClient = {
    async trigger(input) {
      triggerCalls.push(input);
      if (options.trigger) {
        return options.trigger(input, triggerCalls.length);
      }
      const runId = `apirun_${triggerCalls.length}`;
      runStatuses.set(runId, 'in_progress');
      return {
        conversationUrl: `https://chatgpt.com/c/test-${triggerCalls.length}`,
        runId,
      };
    },
    async getRun(runId) {
      if (options.getRun) {
        return options.getRun(runId);
      }
      return { status: runStatuses.get(runId) || 'in_progress', error: null };
    },
  };
  const config = {
    publicBaseUrl: 'http://localhost:8787',
    deviceTokens: [DEVICE_TOKEN],
    capabilityPepper: PEPPER,
    requestTtlMs: options.requestTtlMs || 15 * 60 * 1000,
    retentionMs: 24 * 60 * 60 * 1000,
    rateLimitPerMinute: 100,
    mcpRateLimitPerMinute: 1000,
  };
  const now = options.now || (() => Date.now());
  const store = new RequestStore({ directory, capabilityPepper: PEPPER, now });
  await store.init();
  const app = createApp({ config, store, agentClient, now });
  const server = createServer(app);
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const address = server.address();
  const baseUrl = `http://127.0.0.1:${address.port}`;
  return {
    directory,
    triggerCalls,
    runStatuses,
    store,
    baseUrl,
    async close() {
      await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
      await rm(directory, { recursive: true, force: true });
    },
  };
}

async function mcpFixture(context, t, name) {
  const client = new Client({ name, version: '1.0.0' });
  const transport = new StreamableHTTPClientTransport(new URL(`${context.baseUrl}/mcp`));
  await client.connect(transport);
  t.after(() => transport.close());
  return client;
}

function apiHeaders(idempotencyKey) {
  return {
    Authorization: `Bearer ${DEVICE_TOKEN}`,
    'Content-Type': 'application/json',
    ...(idempotencyKey ? { 'Idempotency-Key': idempotencyKey } : {}),
  };
}

function callbackTokenFromTrigger(trigger) {
  const match = trigger.input.match(/callback_token: ([A-Za-z0-9_-]+)/);
  assert(match, 'trigger prompt must contain the callback capability');
  return match[1];
}

function requestIdFromTrigger(trigger) {
  const match = trigger.input.match(/request_id: (wrq_[A-Za-z0-9_-]+)/);
  assert(match, 'trigger prompt must contain the request ID');
  return match[1];
}

test('API, idempotency, MCP callback, and watch approval work end to end', async (t) => {
  const context = await fixture();
  t.after(() => context.close());

  const unauthorized = await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', 'Idempotency-Key': 'request-unauthorized' },
    body: JSON.stringify({ command: 'test' }),
  });
  assert.equal(unauthorized.status, 401);

  const postBody = JSON.stringify({
    command: 'Create a calendar reminder tomorrow at 15:00',
    utcOffsetMinutes: 240,
  });
  const createdResponse = await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-calendar-001'),
    body: postBody,
  });
  assert.equal(createdResponse.status, 202);
  const created = await createdResponse.json();
  assert.match(created.requestId, /^wrq_/);
  assert.equal(created.status, 'queued');
  assert.equal(context.triggerCalls.length, 1);
  assert.equal(context.triggerCalls[0].conversationKey, `wrist_agent_${created.requestId}`);
  assert.match(context.triggerCalls[0].input, /do not perform the action in this run/i);

  const duplicateResponse = await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-calendar-001'),
    body: postBody,
  });
  assert.equal(duplicateResponse.status, 202);
  assert.equal((await duplicateResponse.json()).requestId, created.requestId);
  assert.equal(context.triggerCalls.length, 1, 'idempotent retry must not retrigger');

  const conflictResponse = await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-calendar-001'),
    body: JSON.stringify({ command: 'Delete every calendar event' }),
  });
  assert.equal(conflictResponse.status, 409);

  const recordPath = path.join(context.directory, 'requests', `${created.requestId}.json`);
  const persisted = await readFile(recordPath, 'utf8');
  assert(!persisted.includes(callbackTokenFromTrigger(context.triggerCalls[0])),
    'plaintext callback capability must not be persisted');

  const mcpClient = new Client({ name: 'wrist-agent-test', version: '1.0.0' });
  const transport = new StreamableHTTPClientTransport(new URL(`${context.baseUrl}/mcp`));
  await mcpClient.connect(transport);
  t.after(() => transport.close());

  const tools = await mcpClient.listTools();
  const callbackTool = tools.tools.find((tool) => tool.name === 'send_to_pebble');
  assert(callbackTool);
  assert.equal(callbackTool.annotations.readOnlyHint, false);
  assert.equal(callbackTool.annotations.idempotentHint, true);

  const proposalArguments = {
    request_id: created.requestId,
    callback_token: callbackTokenFromTrigger(context.triggerCalls[0]),
    short_answer: 'I can create the reminder after you approve it.',
    outcome: 'needs_confirmation',
    action_summary: 'Create calendar reminder tomorrow at 15:00',
  };
  const proposal = await mcpClient.callTool({
    name: 'send_to_pebble',
    arguments: proposalArguments,
  });
  assert.equal(proposal.isError, undefined);
  assert.equal(proposal.structuredContent.accepted, true);

  const proposalReplay = await mcpClient.callTool({
    name: 'send_to_pebble',
    arguments: proposalArguments,
  });
  assert.equal(proposalReplay.structuredContent.duplicate, true);

  const waitingResponse = await fetch(
    `${context.baseUrl}/v1/requests/${created.requestId}`,
    { headers: apiHeaders() },
  );
  const waiting = await waitingResponse.json();
  assert.equal(waiting.status, 'needs_attention');
  assert.equal(waiting.canConfirm, true);
  assert.match(waiting.actionSummary, /calendar reminder/);

  const approvalResponse = await fetch(
    `${context.baseUrl}/v1/requests/${created.requestId}/decision`,
    {
      method: 'POST',
      headers: apiHeaders('approval-calendar-001'),
      body: JSON.stringify({ decision: 'approve' }),
    },
  );
  assert.equal(approvalResponse.status, 202);
  assert.equal(context.triggerCalls.length, 2);
  assert.equal(context.triggerCalls[1].conversationKey, context.triggerCalls[0].conversationKey);
  assert.match(context.triggerCalls[1].input, /explicitly approved/i);
  assert.match(context.triggerCalls[1].input, /APPROVED_ACTION_JSON:/);

  const finalResult = await mcpClient.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: created.requestId,
      callback_token: callbackTokenFromTrigger(context.triggerCalls[1]),
      short_answer: 'Reminder created for tomorrow at 15:00.',
      outcome: 'success',
      action_summary: 'Created one calendar reminder',
    },
  });
  assert.equal(finalResult.structuredContent.status, 'completed');

  const completedResponse = await fetch(
    `${context.baseUrl}/v1/requests/${created.requestId}`,
    { headers: apiHeaders() },
  );
  const completed = await completedResponse.json();
  assert.equal(completed.status, 'completed');
  assert.equal(completed.shortAnswer, 'Reminder created for tomorrow at 15:00.');
  assert.equal(completed.canConfirm, false);
});

test('wrong MCP capability reveals no request details', async (t) => {
  const context = await fixture();
  t.after(() => context.close());

  const created = await (await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-wrong-capability'),
    body: JSON.stringify({ command: 'What is on my calendar?' }),
  })).json();

  const client = new Client({ name: 'wrong-capability-test', version: '1.0.0' });
  const transport = new StreamableHTTPClientTransport(new URL(`${context.baseUrl}/mcp`));
  await client.connect(transport);
  t.after(() => transport.close());
  const result = await client.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: created.requestId,
      callback_token: 'x'.repeat(43),
      short_answer: 'Leaked data',
      outcome: 'success',
    },
  });
  assert.equal(result.isError, true);
  assert(!JSON.stringify(result).includes(created.requestId));
});

test('confirmation requires an exact watch-visible summary', async (t) => {
  const context = await fixture();
  t.after(() => context.close());
  const created = await (await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-summary-contract'),
    body: JSON.stringify({ command: 'Create a reminder' }),
  })).json();
  const client = await mcpFixture(context, t, 'summary-contract-test');
  const baseArguments = {
    request_id: created.requestId,
    callback_token: callbackTokenFromTrigger(context.triggerCalls[0]),
    short_answer: 'Approval is required.',
    outcome: 'needs_confirmation',
  };

  const missing = await client.callTool({
    name: 'send_to_pebble',
    arguments: baseArguments,
  });
  assert.equal(missing.isError, true);

  const hiddenSuffix = await client.callTool({
    name: 'send_to_pebble',
    arguments: { ...baseArguments, action_summary: 'я'.repeat(91) },
  });
  assert.equal(hiddenSuffix.isError, true, 'summary over 180 UTF-8 bytes must be rejected');

  const accepted = await client.callTool({
    name: 'send_to_pebble',
    arguments: { ...baseArguments, action_summary: 'Create one reminder at 15:00' },
  });
  assert.equal(accepted.structuredContent.status, 'needs_confirmation');
});

test('expired confirmation cannot trigger an old action', async (t) => {
  let clock = 1_000_000;
  const context = await fixture({
    now: () => clock,
    requestTtlMs: 1000,
  });
  t.after(() => context.close());
  const created = await (await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-expired-confirmation'),
    body: JSON.stringify({ command: 'Create a reminder' }),
  })).json();
  const client = await mcpFixture(context, t, 'expired-confirmation-test');
  await client.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: created.requestId,
      callback_token: callbackTokenFromTrigger(context.triggerCalls[0]),
      short_answer: 'Approval is required.',
      outcome: 'needs_confirmation',
      action_summary: 'Create one reminder at 15:00',
    },
  });

  clock += 1001;
  const decision = await fetch(
    `${context.baseUrl}/v1/requests/${created.requestId}/decision`,
    {
      method: 'POST',
      headers: apiHeaders('approval-after-expiry'),
      body: JSON.stringify({ decision: 'approve' }),
    },
  );
  assert.equal(decision.status, 409);
  assert.equal((await decision.json()).error.code, 'REQUEST_EXPIRED');
  assert.equal(context.triggerCalls.length, 1, 'expired approval must not trigger a second run');
  assert.equal((await context.store.get(created.requestId)).status, 'expired');
});

test('callback wins over a lost trigger response', async (t) => {
  let rejectTrigger;
  const deferredTrigger = new Promise((resolve, reject) => {
    rejectTrigger = reject;
  });
  const context = await fixture({ trigger: () => deferredTrigger });
  t.after(() => context.close());

  const submissionPromise = fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-lost-trigger-response'),
    body: JSON.stringify({ command: 'What is on my calendar?' }),
  });
  while (context.triggerCalls.length === 0) {
    await new Promise((resolve) => setImmediate(resolve));
  }

  const client = await mcpFixture(context, t, 'trigger-race-test');
  const trigger = context.triggerCalls[0];
  const callback = await client.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: requestIdFromTrigger(trigger),
      callback_token: callbackTokenFromTrigger(trigger),
      short_answer: 'You have two events today.',
      outcome: 'success',
    },
  });
  assert.equal(callback.structuredContent.status, 'completed');

  rejectTrigger(Object.assign(new Error('response lost'), {
    retryable: true,
    code: 'AGENT_NETWORK',
  }));
  assert.equal((await submissionPromise).status, 503);
  const stored = await context.store.get(requestIdFromTrigger(trigger));
  assert.equal(stored.status, 'completed');
  assert.equal(stored.result.shortAnswer, 'You have two events today.');
});

test('parallel approve and reject produce one atomic decision', async (t) => {
  const context = await fixture();
  t.after(() => context.close());
  const created = await (await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-decision-race'),
    body: JSON.stringify({ command: 'Create a reminder' }),
  })).json();
  const client = await mcpFixture(context, t, 'decision-race-test');
  await client.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: created.requestId,
      callback_token: callbackTokenFromTrigger(context.triggerCalls[0]),
      short_answer: 'Approval is required.',
      outcome: 'needs_confirmation',
      action_summary: 'Create one reminder at 15:00',
    },
  });

  const [approve, reject] = await Promise.all([
    fetch(`${context.baseUrl}/v1/requests/${created.requestId}/decision`, {
      method: 'POST',
      headers: apiHeaders('race-approve-001'),
      body: JSON.stringify({ decision: 'approve' }),
    }),
    fetch(`${context.baseUrl}/v1/requests/${created.requestId}/decision`, {
      method: 'POST',
      headers: apiHeaders('race-reject-001'),
      body: JSON.stringify({ decision: 'reject' }),
    }),
  ]);
  const statuses = [approve.status, reject.status];
  assert.equal(statuses.filter((status) => status === 409).length, 1);
  const stored = await context.store.get(created.requestId);
  if (stored.result?.shortAnswer.startsWith('Cancelled')) {
    assert.equal(context.triggerCalls.length, 1);
    assert.equal(stored.result.actionSummary, '');
  } else {
    assert.equal(stored.phase, 'approval');
    assert.equal(context.triggerCalls.length, 2);
  }
});

test('approval phase cannot ask for another watch approval', async (t) => {
  const context = await fixture();
  t.after(() => context.close());
  const created = await (await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-second-confirmation'),
    body: JSON.stringify({ command: 'Create a reminder' }),
  })).json();
  const client = await mcpFixture(context, t, 'second-confirmation-test');
  await client.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: created.requestId,
      callback_token: callbackTokenFromTrigger(context.triggerCalls[0]),
      short_answer: 'Approval is required.',
      outcome: 'needs_confirmation',
      action_summary: 'Create one reminder at 15:00',
    },
  });
  await fetch(`${context.baseUrl}/v1/requests/${created.requestId}/decision`, {
    method: 'POST',
    headers: apiHeaders('approval-second-confirmation'),
    body: JSON.stringify({ decision: 'approve' }),
  });
  const result = await client.callTool({
    name: 'send_to_pebble',
    arguments: {
      request_id: created.requestId,
      callback_token: callbackTokenFromTrigger(context.triggerCalls[1]),
      short_answer: 'Open ChatGPT to complete the remaining confirmation.',
      outcome: 'needs_confirmation',
      action_summary: 'Create one reminder at 15:00',
    },
  });
  assert.equal(result.structuredContent.status, 'needs_chatgpt');
  const status = await (await fetch(`${context.baseUrl}/v1/requests/${created.requestId}`, {
    headers: apiHeaders(),
  })).json();
  assert.equal(status.canConfirm, false);
  assert.equal(status.status, 'needs_attention');
});

test('non-retryable run status errors terminate instead of polling forever', async (t) => {
  const context = await fixture({
    getRun: async () => {
      throw Object.assign(new Error('forbidden'), {
        retryable: false,
        code: 'AGENT_FORBIDDEN',
      });
    },
  });
  t.after(() => context.close());
  const created = await (await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-run-auth-failure'),
    body: JSON.stringify({ command: 'What is on my calendar?' }),
  })).json();
  const status = await (await fetch(`${context.baseUrl}/v1/requests/${created.requestId}`, {
    headers: apiHeaders(),
  })).json();
  assert.equal(status.status, 'failed');
  assert.equal(status.errorCode, 'AGENT_FORBIDDEN');
});

test('cleanup removes request and idempotency metadata together', async (t) => {
  let clock = 10_000;
  const context = await fixture({ now: () => clock });
  t.after(() => context.close());
  await fetch(`${context.baseUrl}/v1/requests`, {
    method: 'POST',
    headers: apiHeaders('request-cleanup-metadata'),
    body: JSON.stringify({ command: 'What is on my calendar?' }),
  });
  clock += 2000;
  assert.equal(await context.store.cleanup(1000), 2);
  assert.deepEqual(await readdir(path.join(context.directory, 'requests')), []);
  assert.deepEqual(await readdir(path.join(context.directory, 'idempotency')), []);
});
