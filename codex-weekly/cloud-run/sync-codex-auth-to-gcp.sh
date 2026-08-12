#!/usr/bin/env bash

# Upload the current Mac Codex auth state directly to Google Secret Manager.
# This is intentionally an opt-in experimental migration, not a supported
# ChatGPT OAuth integration. It does not print, copy to a temporary file, or
# otherwise expose the auth JSON locally.

set -euo pipefail

if [[ "${1:-}" != "--confirm-copy-codex-auth" ]]; then
  cat >&2 <<'USAGE'
Usage:
  ./sync-codex-auth-to-gcp.sh --confirm-copy-codex-auth

This uploads the current local Codex authentication state to Google Secret
Manager. The secret can grant access to the connected ChatGPT account. Review
the Cloud Run README before continuing.
USAGE
  exit 64
fi

if [[ -z "${GCP_PROJECT_ID:-}" ]]; then
  echo "GCP_PROJECT_ID is required. Choose your own Google Cloud project ID." >&2
  exit 64
fi

project_id="$GCP_PROJECT_ID"
secret_id="${CODEX_AUTH_SECRET_ID:-codex-weekly-codex-auth}"
codex_config_directory="${CODEX_HOME:-${HOME}/.codex}"
auth_file="${CODEX_AUTH_FILE:-${codex_config_directory}/auth.json}"
service_account_name="${CLOUD_RUN_SERVICE_ACCOUNT_NAME:-codex-weekly-run}"
service_account_email="${service_account_name}@${project_id}.iam.gserviceaccount.com"

if [[ ! -f "$auth_file" || ! -r "$auth_file" ]]; then
  echo "Local Codex auth file is missing or unreadable: $auth_file" >&2
  exit 1
fi

if ! command -v gcloud >/dev/null 2>&1; then
  echo "gcloud CLI is required. Install it, then run: gcloud auth login" >&2
  exit 1
fi

# Verify the terminal session and exact project before changing cloud state.
gcloud auth print-access-token >/dev/null
gcloud projects describe "$project_id" --format="value(projectNumber)" >/dev/null
gcloud services enable secretmanager.googleapis.com --project="$project_id" >/dev/null

if ! gcloud iam service-accounts describe "$service_account_email" --project="$project_id" >/dev/null 2>&1; then
  gcloud iam service-accounts create "$service_account_name" \
    --project="$project_id" \
    --display-name="Codex Weekly Cloud Run" >/dev/null
fi

if ! gcloud secrets describe "$secret_id" --project="$project_id" >/dev/null 2>&1; then
  gcloud secrets create "$secret_id" --project="$project_id" --replication-policy="automatic" >/dev/null
fi

# gcloud reads the file directly. Neither the JSON nor a derived token appears
# in stdout, a command argument, a temporary file, or this script's logs.
gcloud secrets versions add "$secret_id" --project="$project_id" --data-file="$auth_file" >/dev/null

gcloud secrets add-iam-policy-binding "$secret_id" \
  --project="$project_id" \
  --member="serviceAccount:${service_account_email}" \
  --role="roles/secretmanager.secretAccessor" >/dev/null

printf 'Codex auth state uploaded to Secret Manager: %s (project: %s)\n' \
  "$secret_id" "$project_id"
printf 'Cloud Run service account may read it: %s\n' "$service_account_email"
printf '%s\n' 'Warning: refresh or re-run this script after Codex logout/login or an authorization error.'
