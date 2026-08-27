import path from 'node:path';

function required(name, value) {
  const normalized = String(value || '').trim();
  if (!normalized) {
    throw new Error(`${name} is required`);
  }
  return normalized;
}

function secret(name, value) {
  const normalized = required(name, value);
  if (/^replace(?:-|_)/i.test(normalized) ||
      /^placeholder/i.test(normalized) ||
      /^<.*>$/.test(normalized)) {
    throw new Error(`${name} must not use the example placeholder`);
  }
  return normalized;
}

function integer(name, value, fallback, minimum, maximum) {
  const parsed = value === undefined || value === '' ? fallback : Number(value);
  if (!Number.isInteger(parsed) || parsed < minimum || parsed > maximum) {
    throw new Error(`${name} must be an integer between ${minimum} and ${maximum}`);
  }
  return parsed;
}

function parseDeviceTokens(value) {
  const tokens = required('WRIST_AGENT_DEVICE_TOKENS', value)
    .split(',')
    .map((token) => token.trim())
    .filter(Boolean);
  if (tokens.length === 0 || tokens.some((token) =>
    token.length < 24 || /^replace(?:-|_)/i.test(token) || /^placeholder/i.test(token))) {
    throw new Error('Every WRIST_AGENT_DEVICE_TOKENS entry must be a non-placeholder value containing at least 24 characters');
  }
  return [...new Set(tokens)];
}

function normalizePublicBaseUrl(value) {
  const url = required('PUBLIC_BASE_URL', value).replace(/\/+$/, '');
  if (!/^https:\/\//i.test(url) && !/^http:\/\/localhost(?::\d+)?$/i.test(url)) {
    throw new Error('PUBLIC_BASE_URL must use HTTPS (or localhost for development)');
  }
  const hostname = new URL(url).hostname;
  if (hostname === 'example.com' || hostname.endsWith('.example.com')) {
    throw new Error('PUBLIC_BASE_URL must not use the example placeholder host');
  }
  return url;
}

export function loadConfig(env = process.env) {
  const triggerId = required('WORKSPACE_AGENT_TRIGGER_ID', env.WORKSPACE_AGENT_TRIGGER_ID);
  if (!/^agtch_[A-Za-z0-9_-]+$/.test(triggerId) || triggerId === 'agtch_replace_me') {
    throw new Error('WORKSPACE_AGENT_TRIGGER_ID must be a non-placeholder ID starting with agtch_');
  }

  const pepper = secret('CALLBACK_CAPABILITY_PEPPER', env.CALLBACK_CAPABILITY_PEPPER);
  if (pepper.length < 32) {
    throw new Error('CALLBACK_CAPABILITY_PEPPER must contain at least 32 characters');
  }

  const requestTtlMs = integer(
    'REQUEST_TTL_SECONDS', env.REQUEST_TTL_SECONDS, 900, 120, 3600) * 1000;
  const retentionMs = integer(
    'RETENTION_SECONDS', env.RETENTION_SECONDS, 86400, 900, 604800) * 1000;
  if (retentionMs < requestTtlMs) {
    throw new Error('RETENTION_SECONDS must be greater than or equal to REQUEST_TTL_SECONDS');
  }

  return {
    port: integer('PORT', env.PORT, 8787, 1, 65535),
    dataDir: path.resolve(env.DATA_DIR || './data'),
    publicBaseUrl: normalizePublicBaseUrl(env.PUBLIC_BASE_URL),
    deviceTokens: parseDeviceTokens(env.WRIST_AGENT_DEVICE_TOKENS),
    workspaceAgentTriggerId: triggerId,
    workspaceAgentAccessToken: secret(
      'WORKSPACE_AGENT_ACCESS_TOKEN', env.WORKSPACE_AGENT_ACCESS_TOKEN),
    capabilityPepper: pepper,
    requestTtlMs,
    retentionMs,
    agentTimeoutMs: integer(
      'WORKSPACE_AGENT_TIMEOUT_MS', env.WORKSPACE_AGENT_TIMEOUT_MS, 15000, 1000, 60000),
    agentMaxAttempts: integer(
      'WORKSPACE_AGENT_MAX_ATTEMPTS', env.WORKSPACE_AGENT_MAX_ATTEMPTS, 3, 1, 5),
    rateLimitPerMinute: integer(
      'RATE_LIMIT_PER_MINUTE', env.RATE_LIMIT_PER_MINUTE, 20, 1, 120),
    mcpRateLimitPerMinute: integer(
      'MCP_RATE_LIMIT_PER_MINUTE', env.MCP_RATE_LIMIT_PER_MINUTE, 120, 10, 600),
  };
}
