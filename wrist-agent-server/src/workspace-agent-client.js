const API_BASE_URL = 'https://api.chatgpt.com/v1/workspace_agents';
const RETRYABLE_STATUS = new Set([408, 429, 500, 502, 503, 504]);

export class WorkspaceAgentError extends Error {
  constructor(code, message, { status = 0, retryable = false } = {}) {
    super(message);
    this.name = 'WorkspaceAgentError';
    this.code = code;
    this.status = status;
    this.retryable = retryable;
  }
}

function errorCodeForStatus(status) {
  switch (status) {
    case 401:
      return 'AGENT_AUTH';
    case 403:
      return 'AGENT_FORBIDDEN';
    case 404:
      return 'AGENT_NOT_FOUND';
    case 409:
      return 'AGENT_NOT_RUNNABLE';
    case 429:
      return 'AGENT_RATE_LIMIT';
    default:
      return status >= 500 ? 'AGENT_UNAVAILABLE' : 'AGENT_REQUEST';
  }
}

async function responseJson(response) {
  try {
    return await response.json();
  } catch {
    return null;
  }
}

export class WorkspaceAgentClient {
  constructor({
    triggerId,
    accessToken,
    timeoutMs = 15000,
    maxAttempts = 3,
    fetchImpl = globalThis.fetch,
    sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds)),
    random = Math.random,
  }) {
    this.triggerId = triggerId;
    this.accessToken = accessToken;
    this.timeoutMs = timeoutMs;
    this.maxAttempts = maxAttempts;
    this.fetch = fetchImpl;
    this.sleep = sleep;
    this.random = random;
  }

  headers(extra = {}) {
    return {
      Authorization: `Bearer ${this.accessToken}`,
      Accept: 'application/json',
      ...extra,
    };
  }

  async request(url, options) {
    let lastError;
    for (let attempt = 1; attempt <= this.maxAttempts; attempt += 1) {
      try {
        const response = await this.fetch(url, {
          ...options,
          signal: AbortSignal.timeout(this.timeoutMs),
        });
        if (response.ok) {
          return response;
        }

        const retryable = RETRYABLE_STATUS.has(response.status);
        lastError = new WorkspaceAgentError(
          errorCodeForStatus(response.status),
          `Workspace Agent returned HTTP ${response.status}`,
          { status: response.status, retryable },
        );
        if (!retryable || attempt === this.maxAttempts) {
          throw lastError;
        }
      } catch (error) {
        if (error instanceof WorkspaceAgentError) {
          if (!error.retryable || attempt === this.maxAttempts) {
            throw error;
          }
          lastError = error;
        } else {
          lastError = new WorkspaceAgentError(
            error?.name === 'TimeoutError' ? 'AGENT_TIMEOUT' : 'AGENT_NETWORK',
            'Workspace Agent request failed',
            { retryable: true },
          );
          if (attempt === this.maxAttempts) {
            throw lastError;
          }
        }
      }

      const backoff = Math.round(250 * (2 ** (attempt - 1)) + this.random() * 200);
      await this.sleep(backoff);
    }
    throw lastError;
  }

  async trigger({ input, conversationKey, idempotencyKey }) {
    const response = await this.request(
      `${API_BASE_URL}/${encodeURIComponent(this.triggerId)}/trigger`,
      {
        method: 'POST',
        headers: this.headers({
          'Content-Type': 'application/json',
          'OpenAI-Beta': 'workspace_agent_runs=v1',
          'Idempotency-Key': idempotencyKey,
        }),
        body: JSON.stringify({
          conversation_key: conversationKey,
          input,
        }),
      },
    );
    const body = await responseJson(response);
    if (!body || typeof body.conversation_url !== 'string') {
      throw new WorkspaceAgentError(
        'AGENT_DATA', 'Workspace Agent trigger response is invalid');
    }
    return {
      conversationUrl: body.conversation_url,
      runId: typeof body.agent_trigger_run_id === 'string'
        ? body.agent_trigger_run_id
        : null,
    };
  }

  async getRun(runId) {
    const response = await this.request(
      `${API_BASE_URL}/${encodeURIComponent(this.triggerId)}/runs/${encodeURIComponent(runId)}`,
      {
        method: 'GET',
        headers: this.headers(),
      },
    );
    const body = await responseJson(response);
    if (!body || typeof body.status !== 'string') {
      throw new WorkspaceAgentError(
        'AGENT_DATA', 'Workspace Agent run response is invalid');
    }
    return {
      status: body.status,
      error: body.error || null,
    };
  }
}
