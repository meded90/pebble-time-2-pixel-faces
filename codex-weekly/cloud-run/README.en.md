# Codex Weekly on Google Cloud Run

[Русская версия](README.md)

This guide walks a new user through deploying a **private, user-owned** Codex
Weekly backend. The repository contains no shared Google Cloud project ID,
service URL, user name, email address, or ready-made secret. Every cloud
resource is created in your own Google Cloud project.

This is a Cloud Run service, not a Cloud Function: the container runs the
Codex CLI and communicates with `codex app-server` over stdio.

## What you will deploy

```text
PebbleKit JS ── HTTPS + client token ──> your Cloud Run service
                                                   │
                                                   ├─ Secret Manager: client token
                                                   ├─ Secret Manager: Codex auth copy
                                                   └─ Codex App Server: account/rateLimits/read
```

- `GET /health` publicly reports container readiness.
- `GET /status` requires `Authorization: Bearer <token>` and returns only the
  normalized quota window for `limitId: "codex"`.
- On `503`, the endpoint includes a safe code: `AUTH` (expired Codex login),
  `TIME` (timeout), `DATA` (no main quota), or `ERR`. It never exposes a
  secret or the detailed upstream error.
- The service never exposes credentials, chat history, or arbitrary App Server
  RPC methods.
- Cloud Run scales to zero and is limited to one instance.

## Important authentication limitation

OpenAI documents `account/rateLimits/read` and signing into Codex with ChatGPT.
Copying the local `auth.json` file into Secret Manager is **not** a supported
cloud OAuth contract. It is an opt-in experiment:

- the secret grants access to the connected ChatGPT account;
- you may need to upload it again after `codex logout`, a new login, token
  rotation, a `401` response, or an incompatible Codex update;
- OpenAI may change the file's internal format;
- a multi-user product needs a supported authorization design instead of this
  script.

If this risk is unacceptable, use the local [bridge](../bridge/server.mjs).
It uses the normal local Codex session and does not copy credentials to the
cloud.

Official references:

- [Codex authentication](https://learn.chatgpt.com/docs/auth)
- [Codex App Server](https://learn.chatgpt.com/docs/app-server)
- [Cloud Run: deploy from source](https://cloud.google.com/run/docs/deploying-source-code)
- [Secret Manager: add a secret version](https://cloud.google.com/secret-manager/docs/add-secret-version)
- [Secret Manager: access a secret version](https://cloud.google.com/secret-manager/docs/access-secret-version)

## 1. Install the prerequisites

You need:

- macOS or Linux with Bash;
- the [Google Cloud CLI](https://cloud.google.com/sdk/docs/install);
- the [Codex CLI](https://learn.chatgpt.com/docs/codex/cli);
- `openssl`, `curl`, and Git;
- a Google Cloud account with a billing account;
- a ChatGPT subscription/workspace with Codex access.

Check the commands:

```bash
gcloud --version
codex --version
openssl version
curl --version
```

Run all remaining commands from the root of this repository.

## 2. Choose your own settings

Google Cloud project IDs are globally unique. Replace the example with your
own ID; do not use an email address or full personal name.

```bash
export GCP_PROJECT_ID="your-unique-codex-weekly-project"
export CLOUD_RUN_REGION="us-central1"
export CLOUD_RUN_SERVICE_NAME="codex-weekly"
export CLOUD_RUN_SERVICE_ACCOUNT_NAME="codex-weekly-run"
export CLOUD_BUILD_SERVICE_ACCOUNT_NAME="codex-weekly-build"
```

The scripts intentionally have no default project ID. Keep this terminal open
through the setup because later steps use these variables.

## 3. Create and configure the Google Cloud project

Sign in:

```bash
gcloud auth login
gcloud auth list
```

Create a project, or verify that your chosen project already exists:

```bash
gcloud projects create "$GCP_PROJECT_ID" --name="Codex Weekly"
gcloud config set project "$GCP_PROJECT_ID"
```

If the project already exists, do not create it again. Find your billing
account and link it:

```bash
gcloud billing accounts list
export GCP_BILLING_ACCOUNT_ID="replace-with-your-billing-account-id"
gcloud billing projects link "$GCP_PROJECT_ID" \
  --billing-account="$GCP_BILLING_ACCOUNT_ID"
```

Create a budget alert in Google Cloud Console before deployment. Free tiers
and prices can change, so this guide does not assume zero cost.

## 4. Sign into Codex with ChatGPT

Personal Codex limits require ChatGPT-backed authentication. API-key-only
authentication does not expose this App Server account data.

```bash
codex login
codex login status
```

Complete the browser flow and make sure `codex login status` succeeds.

## 5. Upload a copy of Codex auth to Secret Manager

Read the authentication limitation above first. The script requires an
explicit confirmation flag, reads the local auth file, and sends it directly
to Secret Manager. It does not print the contents or create a temporary copy.

```bash
GCP_PROJECT_ID="$GCP_PROJECT_ID" \
  codex-weekly/cloud-run/sync-codex-auth-to-gcp.sh \
  --confirm-copy-codex-auth
```

By default, the script reads `${CODEX_HOME}/auth.json` when `CODEX_HOME` is
set, otherwise `~/.codex/auth.json`. Set an explicit path if needed:

```bash
export CODEX_AUTH_FILE="/absolute/path/to/auth.json"
```

The script creates:

- the `codex-weekly-run` service account (or your chosen name);
- the `codex-weekly-codex-auth` secret;
- a new secret version containing the current authentication state.

## 6. Deploy the Cloud Run service

```bash
GCP_PROJECT_ID="$GCP_PROJECT_ID" \
CLOUD_RUN_REGION="$CLOUD_RUN_REGION" \
CLOUD_RUN_SERVICE_NAME="$CLOUD_RUN_SERVICE_NAME" \
CLOUD_RUN_SERVICE_ACCOUNT_NAME="$CLOUD_RUN_SERVICE_ACCOUNT_NAME" \
CLOUD_BUILD_SERVICE_ACCOUNT_NAME="$CLOUD_BUILD_SERVICE_ACCOUNT_NAME" \
  codex-weekly/cloud-run/deploy.sh
```

The script:

1. validates the selected project and active `gcloud` login;
2. enables the required Google Cloud APIs;
3. creates dedicated runtime and build service accounts when needed;
4. creates a random client token in `codex-weekly-client-token`;
5. grants the runtime service account access to only the two required secrets;
6. builds the container from `codex-weekly/cloud-run`;
7. deploys with `min-instances=0`, `max-instances=1`, and `concurrency=1`;
8. prints the `/status` URL without printing the token.

`--allow-unauthenticated` applies to Google IAM because PebbleKit JS cannot
present a Google identity. The `/status` handler still requires a separate
bearer token and compares it using a constant-time operation.

## 7. Retrieve the URL and client token

```bash
export CODEX_WEEKLY_SERVICE_URL="$(gcloud run services describe \
  "$CLOUD_RUN_SERVICE_NAME" \
  --project="$GCP_PROJECT_ID" \
  --region="$CLOUD_RUN_REGION" \
  --format='value(status.url)')"

export CODEX_WEEKLY_CLIENT_TOKEN="$(gcloud secrets versions access latest \
  --secret=codex-weekly-client-token \
  --project="$GCP_PROJECT_ID")"
```

Do not print the token or place it in Git, URLs, screenshots, logs, or chats.
Keep the variable only long enough to test the service and paste it into the
PebbleKit settings.

## 8. Test the service

```bash
curl -i "$CODEX_WEEKLY_SERVICE_URL/health"
curl -i "$CODEX_WEEKLY_SERVICE_URL/status"
curl -i \
  -H "Authorization: Bearer $CODEX_WEEKLY_CLIENT_TOKEN" \
  "$CODEX_WEEKLY_SERVICE_URL/status"
```

Expected results:

- `/health` → `200` and `{"ok":true}`;
- `/status` without a token → `401`;
- `/status` with the token → `200` and JSON containing
  `weekly.leftPercent`, `weekly.windowDurationMins`, and `weekly.resetsAt`.

If the authenticated request returns `503`, inspect the logs:

```bash
gcloud run services logs read "$CLOUD_RUN_SERVICE_NAME" \
  --project="$GCP_PROJECT_ID" \
  --region="$CLOUD_RUN_REGION" \
  --limit=100
```

The watchface displays this safe code beside the sync square and clears stale
quota values. `AUTH` means: run `codex login` again, upload a fresh auth secret
in step 5, and deploy a new revision in step 6.

## 9. Connect the watchface

In the Codex Weekly settings on your phone, enter:

```text
Status URL: CODEX_WEEKLY_SERVICE_URL followed by /status
Cloud Run client token: CODEX_WEEKLY_CLIENT_TOKEN
```

Select **Save and sync**. A green indicator means synchronization succeeded.
A red indicator means the URL, token, Codex authentication, or service failed.

Remove the token from the current shell after setup:

```bash
unset CODEX_WEEKLY_CLIENT_TOKEN
```

## Updates and recovery

After code changes, rerun `deploy.sh` with the same variables. After a new
`codex login`, `codex logout`, `401`, or expired authentication, repeat step 5
and then deploy a new revision with step 6 so a new instance reads the secret.

To rotate the client token, add a new secret version and redeploy:

```bash
openssl rand -base64 48 | tr -d '\n' | \
  gcloud secrets versions add codex-weekly-client-token \
    --project="$GCP_PROJECT_ID" \
    --data-file=-

GCP_PROJECT_ID="$GCP_PROJECT_ID" \
CLOUD_RUN_REGION="$CLOUD_RUN_REGION" \
  codex-weekly/cloud-run/deploy.sh
```

Update the watchface settings after rotation.

## Remove the resources

Delete only the resources created for Codex Weekly:

```bash
gcloud run services delete "$CLOUD_RUN_SERVICE_NAME" \
  --project="$GCP_PROJECT_ID" \
  --region="$CLOUD_RUN_REGION"

gcloud secrets delete codex-weekly-client-token --project="$GCP_PROJECT_ID"
gcloud secrets delete codex-weekly-codex-auth --project="$GCP_PROJECT_ID"

gcloud iam service-accounts delete \
  "${CLOUD_RUN_SERVICE_ACCOUNT_NAME}@${GCP_PROJECT_ID}.iam.gserviceaccount.com" \
  --project="$GCP_PROJECT_ID"

gcloud iam service-accounts delete \
  "${CLOUD_BUILD_SERVICE_ACCOUNT_NAME}@${GCP_PROJECT_ID}.iam.gserviceaccount.com" \
  --project="$GCP_PROJECT_ID"
```

Delete the whole project only when it contains no unrelated resources:

```bash
gcloud projects delete "$GCP_PROJECT_ID"
```
