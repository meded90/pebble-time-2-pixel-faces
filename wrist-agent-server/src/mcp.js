import { createHash } from 'node:crypto';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import * as z from 'zod/v4';

class InvalidCapabilityError extends Error {}

function payloadHash(payload) {
  return createHash('sha256').update(JSON.stringify(payload), 'utf8').digest('hex');
}

function normalizeCallback(args) {
  return {
    requestId: args.request_id,
    shortAnswer: args.short_answer.trim(),
    outcome: args.outcome,
    actionSummary: String(args.action_summary || '').trim(),
  };
}

function validateCallback(normalized) {
  if (normalized.outcome === 'needs_confirmation' && !normalized.actionSummary) {
    throw new InvalidCapabilityError();
  }
  if (Buffer.byteLength(normalized.actionSummary, 'utf8') > 180) {
    throw new InvalidCapabilityError();
  }
}

function statusForOutcome(outcome) {
  switch (outcome) {
    case 'success':
    case 'partial':
      return 'completed';
    case 'needs_confirmation':
      return 'needs_confirmation';
    case 'needs_chatgpt':
      return 'needs_chatgpt';
    case 'error':
    default:
      return 'failed';
  }
}

async function applyCallback({ store, now }, args) {
  const normalized = normalizeCallback(args);
  validateCallback(normalized);
  const replayHash = payloadHash(normalized);
  let duplicate = false;

  const updated = await store.update(normalized.requestId, (request) => {
    if (request.callbackExpiresAt <= now() ||
        !store.verifyCapability(request, args.callback_token)) {
      throw new InvalidCapabilityError();
    }
    if (request.callbackPayloadHash) {
      if (request.callbackPayloadHash !== replayHash) {
        throw new InvalidCapabilityError();
      }
      duplicate = true;
      return request;
    }

    const effectiveOutcome = request.phase === 'approval' &&
        normalized.outcome === 'needs_confirmation'
      ? 'needs_chatgpt'
      : normalized.outcome;
    request.callbackPayloadHash = replayHash;
    request.status = statusForOutcome(effectiveOutcome);
    request.result = {
      shortAnswer: normalized.shortAnswer,
      actionSummary: normalized.actionSummary,
      outcome: effectiveOutcome,
    };
    request.errorCode = effectiveOutcome === 'error' ? 'AGENT_REPORTED_ERROR' : null;
    return request;
  });

  if (!updated) {
    throw new InvalidCapabilityError();
  }
  return { request: updated, duplicate };
}

export function createCallbackMcpServer({ store, now = () => Date.now() }) {
  const server = new McpServer(
    { name: 'wrist-agent-callback', version: '1.0.0' },
    {
      instructions:
        'Use send_to_pebble exactly once as the final step of every Wrist Agent API run. The request input supplies a request ID and one-time callback token. Keep answers concise for a 200x228 watch.',
    },
  );

  server.registerTool(
    'send_to_pebble',
    {
      title: 'Send result to Pebble',
      description:
        'Return the final short answer or a proposed action requiring confirmation to the originating Wrist Agent request. action_summary is required and must name the exact change when outcome is needs_confirmation. Call this as the last step of every triggered run.',
      inputSchema: {
        request_id: z.string().regex(/^wrq_[A-Za-z0-9_-]{12,64}$/),
        callback_token: z.string().min(32).max(128),
        short_answer: z.string().trim().min(1).max(600),
        outcome: z.enum([
          'success',
          'partial',
          'needs_confirmation',
          'needs_chatgpt',
          'error',
        ]),
        action_summary: z.string().trim().max(180).optional(),
      },
      outputSchema: {
        accepted: z.boolean(),
        request_id: z.string(),
        status: z.string(),
        duplicate: z.boolean(),
      },
      annotations: {
        readOnlyHint: false,
        destructiveHint: false,
        openWorldHint: false,
        idempotentHint: true,
      },
    },
    async (args) => {
      try {
        const { request, duplicate } = await applyCallback({ store, now }, args);
        const result = {
          accepted: true,
          request_id: request.requestId,
          status: request.status,
          duplicate,
        };
        return {
          structuredContent: result,
          content: [{
            type: 'text',
            text: duplicate
              ? 'This Wrist Agent result was already accepted.'
              : 'Result accepted for delivery to the Pebble watch.',
          }],
        };
      } catch (error) {
        if (!(error instanceof InvalidCapabilityError)) {
          console.error(JSON.stringify({
            level: 'error',
            event: 'mcp_callback_failed',
            message: error?.message || 'unknown error',
          }));
        }
        return {
          isError: true,
          content: [{
            type: 'text',
            text: 'The callback capability is invalid, expired, or already used.',
          }],
        };
      }
    },
  );

  return server;
}

export function createMcpPostHandler(options) {
  return async function mcpPostHandler(request, response, next) {
    const server = createCallbackMcpServer(options);
    const transport = new StreamableHTTPServerTransport({
      sessionIdGenerator: undefined,
    });
    response.on('close', () => {
      transport.close().catch(() => {});
      server.close().catch(() => {});
    });
    try {
      await server.connect(transport);
      await transport.handleRequest(request, response, request.body);
    } catch (error) {
      next(error);
    }
  };
}

export function mcpMethodNotAllowed(request, response) {
  response.status(405).json({
    jsonrpc: '2.0',
    error: { code: -32000, message: 'Method not allowed for stateless MCP' },
    id: null,
  });
}
