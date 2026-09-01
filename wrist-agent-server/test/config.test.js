import assert from 'node:assert/strict';
import test from 'node:test';
import { loadConfig } from '../src/config.js';

function validEnv() {
  return {
    PUBLIC_BASE_URL: 'https://wrist-agent.test.invalid',
    WRIST_AGENT_DEVICE_TOKENS: 'random-device-token-1234567890abcdef',
    CALLBACK_CAPABILITY_PEPPER: 'random-callback-pepper-1234567890abcdef',
    WORKSPACE_AGENT_TRIGGER_ID: 'agtch_live_test_123',
    WORKSPACE_AGENT_ACCESS_TOKEN: 'workspace-agent-access-token-1234567890',
  };
}

test('configuration rejects shipped placeholders', () => {
  const example = {
    PUBLIC_BASE_URL: 'https://agent.example.com',
    WRIST_AGENT_DEVICE_TOKENS: 'replace-with-a-random-device-token',
    CALLBACK_CAPABILITY_PEPPER: 'replace-with-an-independent-random-pepper',
    WORKSPACE_AGENT_TRIGGER_ID: 'agtch_replace_me',
    WORKSPACE_AGENT_ACCESS_TOKEN: 'replace-with-workspace-agent-access-token',
  };
  assert.throws(() => loadConfig(example), /placeholder/i);
});

test('retention cannot expire a still-confirmable request', () => {
  assert.throws(() => loadConfig({
    ...validEnv(),
    REQUEST_TTL_SECONDS: '3600',
    RETENTION_SECONDS: '900',
  }), /RETENTION_SECONDS/);
  const config = loadConfig(validEnv());
  assert.equal(config.requestTtlMs, 900000);
  assert.equal(config.firestoreCollectionPrefix, 'wrist_agent');
});
