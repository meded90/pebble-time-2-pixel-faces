# Wrist Agent on Google Cloud Functions Gen 2

[Русский](GCP_FUNCTIONS.md) · [Main README](README.en.md)

This production deployment option runs the bridge as a Node.js 22 Cloud
Functions Gen 2 HTTP function. Firestore persists requests, idempotency, and
distributed trigger leases across cold starts; Secret Manager holds every
secret.

Use a dedicated GCP project for production. The script grants the required IAM
bindings, but cannot prove that a resource explicitly adopted with
`--force-takeover` has no pre-existing inherited roles.

## First deployment

Install the Google Cloud CLI, authenticate to a billed project where your
identity may enable APIs, create Firestore/secrets/service accounts, grant the
documented IAM roles, and deploy a function. The deploying identity also needs
`Service Account User` on the dedicated runtime account. Create and publish
the Workspace Agent API channel first so that you have its `agtch_...` ID.

```bash
cd wrist-agent-server
npm ci
npm run check
npm test

./scripts/deploy-google-cloud-function.sh \
  --project YOUR_GCP_PROJECT \
  --region europe-west1 \
  --firestore-location eur3 \
  --create-firestore \
  --workspace-agent-trigger-id agtch_YOUR_PUBLISHED_CHANNEL \
  --print-device-token
```

The script asks for the Workspace Agent access token without echoing it. For
CI, pass the token from the CI secret store as
`WORKSPACE_AGENT_ACCESS_TOKEN` and add `--non-interactive`, or pass a
secure `--workspace-agent-token-file`. Write the device token to a protected
CI file/output, not to the build log. Never put an access token in a
command-line argument.

The first run intentionally requires both `--firestore-location` and
`--create-firestore`: a Firestore location is permanent. The script enables
required APIs, creates the Native Firestore database with delete protection,
creates a marked, narrow runtime service account, allows the selected Cloud
Build identity to attach that account, creates/reuses marked secrets, pins
secret versions in the function revision, configures Firestore TTL, deploys
the function, and verifies `/healthz` and `/readyz`.

It prints:

```text
BRIDGE_URL=https://REGION-PROJECT.cloudfunctions.net/wrist-agent-bridge
MCP_URL=https://REGION-PROJECT.cloudfunctions.net/wrist-agent-bridge/mcp
DEVICE_TOKEN=...
```

`--print-device-token` only prints the token to an interactive terminal. In
CI, use `--device-token-output-file /secure/output/device-token.txt`; the
script creates it with mode `0600`. Save the token immediately in a password
manager. The Workspace token and callback pepper are never printed.

## Finish the ChatGPT setup manually

GCP cannot and should not modify a Workspace Agent configuration. In ChatGPT,
create a private MCP connection using `MCP_URL`, attach it to the Workspace
Agent, then republish its API channel. Put `BRIDGE_URL` and `DEVICE_TOKEN`
in the Pebble configuration and test a read-only request before a mutation
confirmation flow. Follow the official [plugin connection
guide](https://developers.openai.com/plugins/deploy/connect-chatgpt).

## Redeploy and rotate

A normal redeploy reuses the current pinned secret versions:

```bash
npm run deploy:gcf -- \
  --project YOUR_GCP_PROJECT \
  --region europe-west1 \
  --workspace-agent-trigger-id agtch_YOUR_PUBLISHED_CHANNEL
```

For a normal redeploy the script deliberately does not print the device token
into a terminal or CI log, even if it was just generated. Add
`--print-device-token` only in an interactive terminal, or use
`--device-token-output-file` in CI.

Use `--rotate-workspace-token` together with a new token file to rotate the
Workspace token. `--rotate-device-token` creates a new Pebble token and
requires updating watch settings; explicitly add `--print-device-token` or a
secure output file to retrieve it. `--rotate-callback-pepper` invalidates
outstanding callbacks, so use it only deliberately.

Firestore TTL setup and deletion are asynchronous; Google says enabling a TTL
policy can take at least ten minutes. The bridge rejects expired requests
immediately; default persistent retention is 24 hours.

## Security notes

The endpoint is intentionally public because Pebble and ChatGPT MCP cannot
present a Google IAM identity. Application authentication remains enforced:
`/v1` requires a device token and `/mcp` requires a single-use callback
capability. No service-account JSON key, local `.env`, or user authentication
file is uploaded. The function uses its dedicated runtime service account via
Application Default Credentials.

The deploy script changes only the explicitly named GCP project. It does not
commit/push Git changes or change Workspace Agent settings. An organization
policy may block public invocation; do not bypass that policy—use an
organization-approved ingress/auth architecture instead.

If the named function, runtime account, or secret already exists without its
`managed-by=wrist-agent` marker, the script stops. Use `--force-takeover` only
after reviewing existing IAM roles and secret versions; it records the marker
for later safe reruns.

For current Google Cloud prerequisites and Firestore locations, see the
[Cloud Functions deployment reference](https://cloud.google.com/sdk/gcloud/reference/functions/deploy),
[Firestore database guide](https://cloud.google.com/firestore/docs/manage-databases),
and [Firestore TTL guide](https://cloud.google.com/firestore/docs/ttl).
