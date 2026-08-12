#!/usr/bin/env bash

# Deploy a Codex Weekly quota bridge to a user-owned Cloud Run project.
set -euo pipefail

if [[ -z "${GCP_PROJECT_ID:-}" ]]; then
  echo "GCP_PROJECT_ID is required. Choose your own Google Cloud project ID." >&2
  exit 64
fi

for command_name in gcloud openssl; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "$command_name is required." >&2
    exit 1
  fi
done

project_id="$GCP_PROJECT_ID"
region="${CLOUD_RUN_REGION:-us-central1}"
service_name="${CLOUD_RUN_SERVICE_NAME:-codex-weekly}"
service_account_name="${CLOUD_RUN_SERVICE_ACCOUNT_NAME:-codex-weekly-run}"
service_account="${CLOUD_RUN_SERVICE_ACCOUNT:-${service_account_name}@${project_id}.iam.gserviceaccount.com}"
build_service_account_name="${CLOUD_BUILD_SERVICE_ACCOUNT_NAME:-codex-weekly-build}"
build_service_account="${build_service_account_name}@${project_id}.iam.gserviceaccount.com"
client_token_secret="${CODEX_WEEKLY_CLIENT_TOKEN_SECRET:-codex-weekly-client-token}"
codex_auth_secret="${CODEX_AUTH_SECRET_ID:-codex-weekly-codex-auth}"
script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

gcloud auth print-access-token >/dev/null
gcloud projects describe "$project_id" --format="value(projectNumber)" >/dev/null
gcloud services enable run.googleapis.com cloudbuild.googleapis.com artifactregistry.googleapis.com secretmanager.googleapis.com \
  --project="$project_id" >/dev/null

if ! gcloud iam service-accounts describe "$service_account" --project="$project_id" >/dev/null 2>&1; then
  gcloud iam service-accounts create "$service_account_name" \
    --project="$project_id" \
    --display-name="Codex Weekly Cloud Run" >/dev/null
fi

if ! gcloud iam service-accounts describe "$build_service_account" --project="$project_id" >/dev/null 2>&1; then
  gcloud iam service-accounts create "$build_service_account_name" \
    --project="$project_id" \
    --display-name="Codex Weekly Cloud Build" >/dev/null
fi

gcloud projects add-iam-policy-binding "$project_id" \
  --member="serviceAccount:${build_service_account}" \
  --role="roles/run.builder" \
  --condition=None >/dev/null

if ! gcloud secrets describe "$codex_auth_secret" --project="$project_id" >/dev/null 2>&1; then
  cat >&2 <<EOF
Codex authentication secret "$codex_auth_secret" does not exist in project "$project_id".
Run this first:
  GCP_PROJECT_ID="$project_id" "$script_directory/sync-codex-auth-to-gcp.sh" --confirm-copy-codex-auth
EOF
  exit 1
fi

if ! gcloud secrets describe "$client_token_secret" --project="$project_id" >/dev/null 2>&1; then
  # The value goes directly from OpenSSL to Secret Manager and is never written
  # to disk or the shell environment. Retrieve it later only when configuring
  # the Pebble watchface.
  gcloud secrets create "$client_token_secret" --project="$project_id" \
    --replication-policy="automatic" >/dev/null
  openssl rand -base64 48 | tr -d '\n' | \
    gcloud secrets versions add "$client_token_secret" --project="$project_id" \
      --data-file=- >/dev/null
fi

for secret_id in "$client_token_secret" "$codex_auth_secret"; do
  gcloud secrets add-iam-policy-binding "$secret_id" --project="$project_id" \
    --member="serviceAccount:${service_account}" \
    --role="roles/secretmanager.secretAccessor" >/dev/null
done

gcloud run deploy "$service_name" \
  --project="$project_id" \
  --region="$region" \
  --source="$script_directory" \
  --build-service-account="projects/${project_id}/serviceAccounts/${build_service_account}" \
  --allow-unauthenticated \
  --service-account="$service_account" \
  --set-secrets="CODEX_WEEKLY_CLIENT_TOKEN=${client_token_secret}:latest,/var/run/secrets/codex/auth.json=${codex_auth_secret}:latest" \
  --min-instances=0 \
  --max-instances=1 \
  --concurrency=1 \
  --cpu=1 \
  --memory=512Mi \
  --timeout=30s

service_url="$(gcloud run services describe "$service_name" \
  --project="$project_id" \
  --region="$region" \
  --format="value(status.url)")"

printf '\nDeployment complete.\n'
printf 'Watchface status URL: %s/status\n' "$service_url"
printf 'Client token secret: %s (project: %s)\n' "$client_token_secret" "$project_id"
printf '%s\n' 'Retrieve the token only when you are ready to paste it into the watchface settings.'
