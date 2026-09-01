# Wrist Agent Server

[Русский](README.md) · [English](README.en.md)

Private HTTPS bridge between PebbleKit JS and a published ChatGPT Workspace
Agent. It triggers the agent, polls run status, and receives a compact result
through the `send_to_pebble` MCP tool.

The callback is required because the Workspace Agents API currently returns an
accepted event, conversation URL, and optional beta run ID, but not the agent's
response text. See the official [trigger-runs documentation](https://developers.openai.com/workspace-agents/trigger-runs).

## Setup

1. Ask a workspace administrator to enable Workspace Agents and access tokens.
2. Create an agent, attach only the required apps/connectors, and add
   [`AGENT_INSTRUCTIONS.md`](AGENT_INSTRUCTIONS.md).
3. Publish its API channel and copy the stable `agtch_...` ID.
4. Create a Workspace Agent access token as described in the official
   [authentication guide](https://developers.openai.com/workspace-agents/authentication).
5. Copy `.env.example` to `.env`, generate independent device-token and
   capability-pepper secrets, and fill every required value.
6. Run `npm ci && npm run check && npm test`.
7. Choose either the managed Cloud Functions Gen 2 deployment or one Docker
   replica behind HTTPS with a persistent `/app/data` volume.
8. In ChatGPT developer mode, connect the resulting MCP URL, verify the
   `send_to_pebble` tool, attach the private plugin to the agent, and republish
   the API channel. Follow [Connect and test your plugin](https://developers.openai.com/plugins/deploy/connect-chatgpt).

## Deploy on Cloud Functions Gen 2

The recommended serverless option persists requests, idempotency, and trigger
leases in Firestore, while Secret Manager holds the Workspace Agent token,
Pebble device token, and callback pepper. One script configures the required
GCP resources and verifies the deployment:

```bash
./scripts/deploy-google-cloud-function.sh \
  --project YOUR_GCP_PROJECT \
  --region europe-west1 \
  --firestore-location eur3 \
  --create-firestore \
  --workspace-agent-trigger-id agtch_YOUR_PUBLISHED_CHANNEL \
  --print-device-token
```

It accepts the Workspace Agent token through hidden terminal input and prints
`MCP_URL`; the device token is printed only with explicit
`--print-device-token` in an interactive terminal. Read
[GCP_FUNCTIONS.en.md](GCP_FUNCTIONS.en.md) before running it: Firestore
location is permanent, the guide documents required IAM, safe CI input,
rotation, and the manual ChatGPT connection step.

## Deploy one Docker replica

```bash
docker compose up --build -d
curl https://YOUR-HOST/healthz
npx @modelcontextprotocol/inspector@latest
```

Use the Inspector's Streamable HTTP mode with `https://YOUR-HOST/mcp`.

The example values intentionally fail validation until every placeholder is
replaced. `npm start` loads the local `.env` with Node's built-in `--env-file`.
Compose binds plaintext port 8787 to host loopback only; terminate HTTPS on the
same host. If the reverse proxy is another container, use a shared private
Docker network instead of exposing the plaintext port.

## Security and persistence

The Workspace Agent token never leaves the server. Phone requests use a
separate bearer token. MCP callbacks use expiring per-request capabilities;
plaintext capabilities are not stored after the upstream trigger is accepted.
Files are written with mode `0600` and atomic rename.

Accepted dictation is retained only while a trigger is pending or retryable.
Compact status and result records are kept for 24 hours by default. Run a
single replica with the included file store; use a transactional database for
multiple replicas. Read [`SECURITY.md`](SECURITY.md) before enabling write tools.

This private capability pattern is not a substitute for OAuth in a public,
multi-tenant OpenAI plugin. Current public-plugin authentication expectations
are documented in [MCP authentication](https://developers.openai.com/plugins/build/auth).

## Endpoints

- `GET /healthz`, `GET /readyz`
- `POST /v1/requests` with device bearer token and `Idempotency-Key`
- `GET /v1/requests/:id`
- `POST /v1/requests/:id/decision` with `approve` or `reject`
- `POST /mcp` using stateless Streamable HTTP

An upstream `suspended` run cannot currently be resumed through the documented
Workspace Agents API. The watch therefore asks the user to open ChatGPT.

Live end-to-end validation still requires a real published API channel,
Workspace Agent access token, public HTTPS MCP endpoint, and connected apps.
