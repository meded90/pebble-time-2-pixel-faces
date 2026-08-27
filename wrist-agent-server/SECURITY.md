# Security and privacy boundary

## Secrets

- `WORKSPACE_AGENT_ACCESS_TOKEN` is accepted only from the server environment.
  It is never returned by an endpoint or sent to PebbleKit JS.
- `WRIST_AGENT_DEVICE_TOKENS` authenticates phone-to-bridge traffic. Generate a
  separate random value for each user or watch.
- `CALLBACK_CAPABILITY_PEPPER` protects one-time callback capabilities at rest.
  Each callback token is HMACed; while a trigger is pending, a recoverable copy
  is AES-256-GCM sealed so an idempotent retry can survive a process restart.
- Put production values in a secret manager. Never commit `.env`.

## Stored data

The file store uses mode `0600` and atomic rename. Accepted dictation text is
kept only while an upstream trigger is pending or retryable, then removed after
the trigger is accepted. Compact status, conversation URL, result, and action
summary are retained for `RETENTION_SECONDS` (24 hours by default). ChatGPT and
connected-app retention is governed separately by the user's workspace.

Use one process/replica with a persistent volume. For multiple replicas, replace
the file store with a transactional database and conditional updates.

## MCP callback

`/mcp` is intentionally stateless and its tool catalog contains no user data.
The state-changing `send_to_pebble` tool requires a request-scoped, expiring
capability in its validated input. Identical replay is idempotent; conflicting
replay is rejected without revealing whether a request exists. A connection-IP
rate limit also bounds unauthenticated MCP discovery and malformed calls.

This capability model is intended for a private developer-mode plugin attached
to the owner's Workspace Agent. A publicly distributed OpenAI plugin that
serves multiple customers should implement OAuth 2.1, tenant isolation,
organization verification, and the current OpenAI plugin review requirements.

## Action confirmation

The initial trigger instructs the agent to propose consequential writes before
executing them. Select on the watch sends a second, idempotent approval trigger
in the same conversation. This improves UX and reduces accidental actions, but
prompt instructions alone cannot technically prevent a connected write tool
from being called early. For a hard boundary, keep native connectors read-only
or put writes behind a bridge-owned tool that checks the stored approval.

Before publishing, the owner must add a real private security contact to the
store listing. Do not include tokens, transcripts, or personal data in a public
issue.
