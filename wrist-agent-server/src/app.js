import { randomBytes } from 'node:crypto';
import express from 'express';
import {
  capabilityHash,
  openSecret,
  safeEqualHex,
  sealSecret,
  sha256,
} from './store.js';
import { createMcpPostHandler, mcpMethodNotAllowed } from './mcp.js';

const ACTIVE_STATUSES = new Set([
  'trigger_pending',
  'trigger_retryable',
  'queued',
  'in_progress',
  'awaiting_callback',
]);

const TERMINAL_STATUSES = new Set(['completed', 'failed', 'expired']);

function constantTimeTokenMatch(candidate, expected) {
  const left = sha256(candidate);
  const right = sha256(expected);
  return safeEqualHex(left, right);
}

function authenticateRequest(request, deviceTokens) {
  const authorization = String(request.get('authorization') || '');
  const match = authorization.match(/^Bearer\s+(.+)$/i);
  if (!match) {
    return null;
  }
  const candidate = match[1].trim();
  for (const token of deviceTokens) {
    if (constantTimeTokenMatch(candidate, token)) {
      return sha256(token);
    }
  }
  return null;
}

function createRequestId() {
  return `wrq_${randomBytes(12).toString('base64url')}`;
}

function createCallbackToken() {
  return randomBytes(32).toString('base64url');
}

function validateIdempotencyKey(request) {
  const key = String(request.get('idempotency-key') || '').trim();
  if (!/^[A-Za-z0-9._:-]{8,128}$/.test(key)) {
    return null;
  }
  return key;
}

function cleanCommand(value) {
  const command = String(value || '').replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F\u007F]/g, '').trim();
  if (!command || command.length > 1000) {
    return null;
  }
  return command;
}

function promptForRequest(request, callbackToken, publicBaseUrl) {
  const callbackContract = [
    'MANDATORY CALLBACK: As the final step of this run, call the Wrist Agent Callback tool send_to_pebble.',
    `request_id: ${request.requestId}`,
    `callback_token: ${callbackToken}`,
    `MCP endpoint configured for this agent: ${publicBaseUrl}/mcp`,
    'Keep short_answer under 600 characters. action_summary must fit in 180 UTF-8 bytes so the watch can show the exact approved action.',
    'Use outcome success, partial, needs_confirmation, needs_chatgpt, or error.',
  ].join('\n');

  if (request.phase === 'approval') {
    return [
      'WRIST AGENT APPROVAL RUN',
      'The user explicitly approved the exact proposed action on their Pebble watch.',
      `APPROVED_ACTION_JSON: ${JSON.stringify(request.approvedActionSummary)}`,
      'Continue the same conversation, perform only that approved action, then report the actual result.',
      'If the connected tool still requires ChatGPT-host confirmation, report needs_chatgpt instead of claiming completion.',
      callbackContract,
    ].join('\n\n');
  }

  return [
    'WRIST AGENT VOICE REQUEST',
    'The text below is an accepted Pebble dictation command. Treat it as the user request.',
    `Phone UTC offset in minutes: ${request.utcOffsetMinutes}`,
    `USER_COMMAND_JSON: ${JSON.stringify(request.command)}`,
    'For read-only questions or harmless lookups, complete the work and return the concise result.',
    'Before creating, changing, deleting, sending, purchasing, or otherwise mutating external data, do not perform the action in this run. First call send_to_pebble with outcome needs_confirmation and an exact action_summary, then end the run. A later run will state whether the user approved it.',
    'If you cannot proceed without opening ChatGPT, return outcome needs_chatgpt. Never claim an action happened unless its tool call succeeded.',
    callbackContract,
  ].join('\n\n');
}

function publicStatus(request) {
  switch (request.status) {
    case 'trigger_pending':
    case 'trigger_retryable':
    case 'queued':
      return 'queued';
    case 'in_progress':
    case 'awaiting_callback':
      return 'in_progress';
    case 'needs_confirmation':
    case 'needs_chatgpt':
      return 'needs_attention';
    default:
      return request.status;
  }
}

function publicRequest(request, currentTime = Date.now()) {
  const retryDelay = request.nextRunPollAt
    ? Math.max(0, request.nextRunPollAt - currentTime)
    : 0;
  return {
    requestId: request.requestId,
    status: publicStatus(request),
    shortAnswer: request.result?.shortAnswer || null,
    actionSummary: request.result?.actionSummary || null,
    canConfirm: request.status === 'needs_confirmation',
    conversationUrl: request.conversationUrl || null,
    errorCode: request.errorCode || null,
    pollAfterMs: ACTIVE_STATUSES.has(request.status)
      ? Math.max(3000, retryDelay)
      : null,
    expiresAt: new Date(request.expiresAt).toISOString(),
  };
}

function setAcceptedHeaders(response, request) {
  response.set('Location', `/v1/requests/${request.requestId}`);
  response.set('Retry-After', '3');
}

class MinuteRateLimiter {
  constructor(limit, now) {
    this.limit = limit;
    this.now = now;
    this.buckets = new Map();
  }

  allow(principal) {
    const minute = Math.floor(this.now() / 60000);
    const bucket = this.buckets.get(principal);
    if (!bucket || bucket.minute !== minute) {
      this.buckets.set(principal, { minute, count: 1 });
      return true;
    }
    bucket.count += 1;
    return bucket.count <= this.limit;
  }
}

async function triggerStoredRequest({ request, store, agentClient, config, now }) {
  return store.withLock(`trigger:${request.requestId}`, async () => {
    let currentRequest = await store.get(request.requestId);
    if (!currentRequest) {
      return null;
    }
    if (!['trigger_pending', 'trigger_retryable'].includes(currentRequest.status)) {
      return currentRequest;
    }
    if (currentRequest.expiresAt <= now()) {
      return store.update(currentRequest.requestId, (current) => {
        if (['trigger_pending', 'trigger_retryable'].includes(current.status) &&
            current.expiresAt <= now()) {
          current.status = 'expired';
          current.errorCode = 'REQUEST_EXPIRED';
          current.pendingCallbackToken = null;
          current.callbackCapabilityHash = null;
          current.command = null;
        }
        return current;
      });
    }

    const callbackToken = openSecret(
      config.capabilityPepper, currentRequest.pendingCallbackToken);
    const prompt = promptForRequest(currentRequest, callbackToken, config.publicBaseUrl);
    try {
      const trigger = await agentClient.trigger({
        input: prompt,
        conversationKey: currentRequest.conversationKey,
        idempotencyKey: currentRequest.agentIdempotencyKey,
      });
      const acceptedAt = now();
      return store.update(currentRequest.requestId, (current) => {
        current.conversationUrl = trigger.conversationUrl;
        current.agentRunId = trigger.runId;
        current.upstreamStatus = trigger.runId ? 'queued' : 'unknown';
        current.lastRunPollAt = 0;
        current.nextRunPollAt = 0;
        current.runPollFailures = 0;
        current.pendingCallbackToken = null;
        current.command = null;
        if (['trigger_pending', 'trigger_retryable'].includes(current.status)) {
          current.status = trigger.runId ? 'queued' : 'awaiting_callback';
        }
        current.callbackExpiresAt = acceptedAt + config.requestTtlMs;
        current.expiresAt = acceptedAt + config.requestTtlMs;
        return current;
      });
    } catch (error) {
      const retryable = error?.retryable === true;
      const updated = await store.update(currentRequest.requestId, (current) => {
        if (!['trigger_pending', 'trigger_retryable'].includes(current.status)) {
          return current;
        }
        current.status = retryable ? 'trigger_retryable' : 'failed';
        current.errorCode = error?.code || 'AGENT_REQUEST';
        if (!retryable) {
          current.pendingCallbackToken = null;
          current.callbackCapabilityHash = null;
          current.command = null;
        }
        return current;
      });
      const wrapped = new Error('Workspace Agent trigger failed');
      wrapped.statusCode = retryable ? 503 : 502;
      wrapped.publicCode = error?.code || updated.errorCode || 'AGENT_REQUEST';
      throw wrapped;
    }
  });
}

async function refreshRun({ request, store, agentClient, now }) {
  if (!request.agentRunId || !ACTIVE_STATUSES.has(request.status) ||
      request.lastRunPollAt > now() - 2000 || request.nextRunPollAt > now()) {
    return request;
  }

  let run;
  try {
    run = await agentClient.getRun(request.agentRunId);
  } catch (error) {
    return store.update(request.requestId, (current) => {
      if (!ACTIVE_STATUSES.has(current.status)) {
        return current;
      }
      current.lastRunPollAt = now();
      current.lastPollErrorCode = error?.code || 'AGENT_STATUS';
      if (error?.retryable === false) {
        current.status = 'failed';
        current.errorCode = current.lastPollErrorCode;
        current.result ||= {
          shortAnswer: 'Workspace Agent status could not be read. Check the server credentials and published API channel.',
          actionSummary: '',
          outcome: 'error',
        };
        current.nextRunPollAt = 0;
        return current;
      }
      current.runPollFailures = (current.runPollFailures || 0) + 1;
      const backoff = Math.min(
        60000,
        3000 * (2 ** Math.min(current.runPollFailures - 1, 4)),
      );
      current.nextRunPollAt = now() + backoff;
      return current;
    });
  }

  return store.update(request.requestId, (current) => {
    current.lastRunPollAt = now();
    current.upstreamStatus = run.status;
    current.lastPollErrorCode = null;
    current.nextRunPollAt = 0;
    current.runPollFailures = 0;
    if (!ACTIVE_STATUSES.has(current.status)) {
      return current;
    }
    switch (run.status) {
      case 'queued':
        current.status = 'queued';
        break;
      case 'in_progress':
        current.status = 'in_progress';
        break;
      case 'suspended':
        current.status = 'needs_chatgpt';
        current.errorCode = 'OPEN_CHATGPT';
        current.result ||= {
          shortAnswer: 'Open the ChatGPT conversation to continue or approve this request.',
          actionSummary: '',
          outcome: 'needs_chatgpt',
        };
        break;
      case 'completed':
        current.status = 'awaiting_callback';
        break;
      case 'failed':
        current.status = 'failed';
        current.errorCode = 'AGENT_FAILED';
        current.result ||= {
          shortAnswer: 'The Workspace Agent run failed.',
          actionSummary: '',
          outcome: 'error',
        };
        break;
      default:
        break;
    }
    return current;
  });
}

async function expireIfNeeded(request, store, now) {
  if (!TERMINAL_STATUSES.has(request.status) && request.expiresAt <= now()) {
    return store.update(request.requestId, (current) => {
      if (!TERMINAL_STATUSES.has(current.status) && current.expiresAt <= now()) {
        const previousStatus = current.status;
        current.status = 'expired';
        current.errorCode = previousStatus === 'awaiting_callback'
          ? 'CALLBACK_MISSING'
          : 'REQUEST_EXPIRED';
        current.pendingCallbackToken = null;
        current.callbackCapabilityHash = null;
        current.command = null;
      }
      return current;
    });
  }
  return request;
}

export function createApp({ config, store, agentClient, now = () => Date.now() }) {
  const app = express();
  const limiter = new MinuteRateLimiter(config.rateLimitPerMinute, now);
  const mcpLimiter = new MinuteRateLimiter(config.mcpRateLimitPerMinute || 120, now);

  app.disable('x-powered-by');
  app.use((request, response, next) => {
    response.set({
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Headers': 'Authorization, Content-Type, Idempotency-Key',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Cache-Control': 'no-store',
      'Content-Security-Policy': "default-src 'none'; frame-ancestors 'none'",
      'Referrer-Policy': 'no-referrer',
      'X-Content-Type-Options': 'nosniff',
    });
    if (request.method === 'OPTIONS') {
      response.status(204).end();
      return;
    }
    next();
  });
  app.use(express.json({ limit: '16kb', type: ['application/json', 'application/*+json'] }));

  app.get('/', (request, response) => {
    response.json({
      name: 'wrist-agent-server',
      version: '1.0.0',
      status: 'ok',
      mcp: `${config.publicBaseUrl}/mcp`,
    });
  });
  app.get('/healthz', (request, response) => response.json({ status: 'ok' }));
  app.get('/readyz', async (request, response, next) => {
    try {
      await store.init();
      response.json({ status: 'ready' });
    } catch (error) {
      next(error);
    }
  });

  app.post('/mcp', (request, response, next) => {
    const principal = String(request.socket.remoteAddress || 'unknown');
    if (!mcpLimiter.allow(principal)) {
      response.set('Retry-After', '60');
      response.status(429).json({
        jsonrpc: '2.0',
        error: { code: -32001, message: 'MCP rate limit exceeded' },
        id: null,
      });
      return;
    }
    next();
  }, createMcpPostHandler({ store, now }));
  app.get('/mcp', mcpMethodNotAllowed);
  app.delete('/mcp', mcpMethodNotAllowed);

  app.use('/v1', (request, response, next) => {
    const principalHash = authenticateRequest(request, config.deviceTokens);
    if (!principalHash) {
      response.status(401).json({ error: { code: 'AUTH', message: 'Unauthorized' } });
      return;
    }
    if (!limiter.allow(principalHash)) {
      response.set('Retry-After', '60');
      response.status(429).json({ error: { code: 'RATE_LIMIT', message: 'Too many requests' } });
      return;
    }
    request.wristAgentPrincipalHash = principalHash;
    next();
  });

  app.post('/v1/requests', async (request, response, next) => {
    try {
      const idempotencyKey = validateIdempotencyKey(request);
      const command = cleanCommand(request.body?.command);
      if (!idempotencyKey || !command) {
        response.status(400).json({
          error: { code: 'VALIDATION', message: 'Valid command and Idempotency-Key are required' },
        });
        return;
      }

      const utcOffsetMinutes = Number.isInteger(request.body?.utcOffsetMinutes)
        ? Math.max(-840, Math.min(840, request.body.utcOffsetMinutes))
        : 0;
      const bodyHash = sha256(JSON.stringify({ command, utcOffsetMinutes }));
      const idempotencyHash = sha256(
        `${request.wristAgentPrincipalHash}:${idempotencyKey}`);
      const existing = await store.getByIdempotency(idempotencyHash);
      if (existing) {
        if (existing.mapping.principalHash !== request.wristAgentPrincipalHash ||
            existing.mapping.bodyHash !== bodyHash) {
          response.status(409).json({
            error: { code: 'IDEMPOTENCY_CONFLICT', message: 'Idempotency key was used for another request' },
          });
          return;
        }
        const resumed = await triggerStoredRequest({
          request: existing.request, store, agentClient, config, now,
        });
        setAcceptedHeaders(response, resumed);
        response.status(202).json(publicRequest(resumed, now()));
        return;
      }

      const callbackToken = createCallbackToken();
      const requestId = createRequestId();
      const timestamp = now();
      const record = {
        schemaVersion: 1,
        requestId,
        principalHash: request.wristAgentPrincipalHash,
        conversationKey: `wrist_agent_${requestId}`,
        phase: 'initial',
        command,
        utcOffsetMinutes,
        status: 'trigger_pending',
        callbackCapabilityHash: capabilityHash(config.capabilityPepper, callbackToken),
        callbackExpiresAt: timestamp + config.requestTtlMs,
        callbackPayloadHash: null,
        pendingCallbackToken: sealSecret(config.capabilityPepper, callbackToken),
        agentIdempotencyKey: `${requestId}-initial`,
        agentRunId: null,
        conversationUrl: null,
        upstreamStatus: null,
        result: null,
        errorCode: null,
        lastRunPollAt: 0,
        nextRunPollAt: 0,
        runPollFailures: 0,
        createdAt: timestamp,
        updatedAt: timestamp,
        expiresAt: timestamp + config.requestTtlMs,
      };
      const created = await store.create(record, {
        hash: idempotencyHash,
        bodyHash,
      });
      if (!created.created) {
        if (created.mapping.principalHash !== request.wristAgentPrincipalHash ||
            created.mapping.bodyHash !== bodyHash) {
          response.status(409).json({
            error: { code: 'IDEMPOTENCY_CONFLICT', message: 'Idempotency key was used for another request' },
          });
          return;
        }
        const resumed = await triggerStoredRequest({
          request: created.request, store, agentClient, config, now,
        });
        setAcceptedHeaders(response, resumed);
        response.status(202).json(publicRequest(resumed, now()));
        return;
      }

      const triggered = await triggerStoredRequest({
        request: record, store, agentClient, config, now,
      });
      setAcceptedHeaders(response, triggered);
      response.status(202).json(publicRequest(triggered, now()));
    } catch (error) {
      next(error);
    }
  });

  app.get('/v1/requests/:requestId', async (request, response, next) => {
    try {
      let record = await store.get(request.params.requestId);
      if (!record || record.principalHash !== request.wristAgentPrincipalHash) {
        response.status(404).json({ error: { code: 'NOT_FOUND', message: 'Request not found' } });
        return;
      }
      record = await expireIfNeeded(record, store, now);
      record = await refreshRun({ request: record, store, agentClient, now });
      response.json(publicRequest(record, now()));
    } catch (error) {
      next(error);
    }
  });

  app.post('/v1/requests/:requestId/decision', async (request, response, next) => {
    try {
      const idempotencyKey = validateIdempotencyKey(request);
      const decision = request.body?.decision;
      if (!idempotencyKey || !['approve', 'reject'].includes(decision)) {
        response.status(400).json({
          error: { code: 'VALIDATION', message: 'Decision must be approve or reject' },
        });
        return;
      }

      const decisionHash = sha256(`${request.wristAgentPrincipalHash}:${idempotencyKey}:${decision}`);
      const callbackToken = decision === 'approve' ? createCallbackToken() : null;
      let record = await store.update(request.params.requestId, (current) => {
        if (current.principalHash !== request.wristAgentPrincipalHash) {
          const error = new Error('Request not found');
          error.statusCode = 404;
          error.publicCode = 'NOT_FOUND';
          throw error;
        }
        if (!TERMINAL_STATUSES.has(current.status) && current.expiresAt <= now()) {
          current.status = 'expired';
          current.errorCode = 'REQUEST_EXPIRED';
          current.pendingCallbackToken = null;
          current.callbackCapabilityHash = null;
          current.command = null;
          return current;
        }
        if (current.decisionHash) {
          if (current.decisionHash !== decisionHash) {
            const error = new Error('A different decision was already submitted');
            error.statusCode = 409;
            error.publicCode = 'DECISION_CONFLICT';
            throw error;
          }
          return current;
        }
        if (current.status !== 'needs_confirmation') {
          const error = new Error('Request is not waiting for a decision');
          error.statusCode = 409;
          error.publicCode = 'NOT_CONFIRMABLE';
          throw error;
        }

        if (decision === 'reject') {
          current.decisionHash = decisionHash;
          current.status = 'completed';
          current.result = {
            shortAnswer: 'Cancelled. No approved action was performed.',
            actionSummary: '',
            outcome: 'success',
          };
          current.callbackCapabilityHash = null;
          current.pendingCallbackToken = null;
          return current;
        }

        const approvedActionSummary = String(current.result?.actionSummary || '').trim();
        if (!approvedActionSummary || Buffer.byteLength(approvedActionSummary, 'utf8') > 180) {
          const error = new Error('The proposed action summary is not watch-visible');
          error.statusCode = 409;
          error.publicCode = 'INVALID_ACTION_SUMMARY';
          throw error;
        }
        current.decisionHash = decisionHash;
        current.phase = 'approval';
        current.approvedActionSummary = approvedActionSummary;
        current.status = 'trigger_pending';
        current.callbackCapabilityHash = capabilityHash(config.capabilityPepper, callbackToken);
        current.callbackExpiresAt = now() + config.requestTtlMs;
        current.callbackPayloadHash = null;
        current.pendingCallbackToken = sealSecret(config.capabilityPepper, callbackToken);
        current.agentIdempotencyKey = `${current.requestId}-approval-1`;
        current.agentRunId = null;
        current.upstreamStatus = null;
        current.errorCode = null;
        current.nextRunPollAt = 0;
        current.runPollFailures = 0;
        current.expiresAt = now() + config.requestTtlMs;
        return current;
      });

      if (!record) {
        response.status(404).json({ error: { code: 'NOT_FOUND', message: 'Request not found' } });
        return;
      }
      if (record.status === 'expired' && !record.decisionHash) {
        response.status(409).json({
          error: { code: 'REQUEST_EXPIRED', message: 'The confirmation request expired' },
        });
        return;
      }
      if (decision === 'reject') {
        response.status(200).json(publicRequest(record, now()));
        return;
      }

      if (['trigger_pending', 'trigger_retryable'].includes(record.status)) {
        record = await triggerStoredRequest({ request: record, store, agentClient, config, now });
      }
      setAcceptedHeaders(response, record);
      response.status(202).json(publicRequest(record, now()));
    } catch (error) {
      next(error);
    }
  });

  app.use((request, response) => {
    response.status(404).json({ error: { code: 'NOT_FOUND', message: 'Route not found' } });
  });

  app.use((error, request, response, next) => {
    if (response.headersSent) {
      next(error);
      return;
    }
    const parseFailed = error?.type === 'entity.parse.failed';
    const status = error?.type === 'entity.too.large'
      ? 413
      : error?.statusCode || error?.status || 500;
    const code = error?.type === 'entity.too.large'
      ? 'BODY_TOO_LARGE'
      : parseFailed ? 'INVALID_JSON' : error?.publicCode || 'INTERNAL';
    if (status >= 500) {
      console.error(JSON.stringify({
        level: 'error',
        event: 'http_request_failed',
        code,
        path: request.path,
        message: error?.message || 'unknown error',
      }));
    }
    response.status(status).json({ error: { code, message: status >= 500 ? 'Service unavailable' : 'Request failed' } });
  });

  return app;
}
