#!/usr/bin/env bash
# Deploy Wrist Agent as a public Cloud Functions Gen 2 endpoint.
# Secrets never enter the source tree or command-line arguments.
set -Eeuo pipefail
IFS=$'\n\t'
umask 077

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

PROJECT_ID="${GCP_PROJECT_ID:-}"
REGION="${GCP_REGION:-}"
FIRESTORE_LOCATION="${FIRESTORE_LOCATION:-}"
FUNCTION_NAME="wrist-agent-bridge"
RUNTIME_SERVICE_ACCOUNT_ID="wrist-agent-runtime"
SECRET_PREFIX=""
COLLECTION_PREFIX="wrist_agent"
WORKSPACE_AGENT_TRIGGER_ID="${WORKSPACE_AGENT_TRIGGER_ID:-}"
WORKSPACE_TOKEN_FILE=""
CREATE_FIRESTORE=false
FORCE_TAKEOVER=false
NON_INTERACTIVE=false
ROTATE_WORKSPACE_TOKEN=false
ROTATE_DEVICE_TOKEN=false
ROTATE_CALLBACK_PEPPER=false
PRINT_DEVICE_TOKEN=false
DEVICE_TOKEN_OUTPUT_FILE=""
MAX_INSTANCES=4
CONCURRENCY=20
FUNCTION_TIMEOUT="120s"

usage() {
  cat <<'USAGE'
Usage:
  ./scripts/deploy-google-cloud-function.sh \
    --project PROJECT_ID \
    --region REGION \
    --firestore-location LOCATION \
    --create-firestore \
    --workspace-agent-trigger-id agtch_... \
    --workspace-agent-token-file /secure/path/token.txt

Required on the first deployment:
  --project ID                       Google Cloud project ID (or GCP_PROJECT_ID).
  --region REGION                    Cloud Functions Gen 2 region.
  --firestore-location LOCATION      Immutable location for the default Firestore DB.
  --create-firestore                 Explicitly authorize creating that DB when absent.
  --workspace-agent-trigger-id ID    Published ChatGPT Workspace Agent API channel ID.
  --workspace-agent-token-file FILE  File containing the Workspace Agent access token.

Useful optional flags:
  --function-name NAME               Default: wrist-agent-bridge.
  --runtime-service-account-id ID    Default: wrist-agent-runtime.
  --secret-prefix PREFIX             Default: function name.
  --firestore-collection-prefix ID   Default: wrist_agent.
  --max-instances N                  Default: 4.
  --concurrency N                    Default: 20.
  --function-timeout DURATION        Default: 120s.
  --rotate-workspace-token           Add a version from --workspace-agent-token-file.
  --rotate-device-token              Generate a new Pebble device token.
  --rotate-callback-pepper           Generate a new callback pepper (invalidates pending callbacks).
  --print-device-token               Print the device token only to an interactive terminal.
  --device-token-output-file FILE    Write the device token to FILE with mode 0600.
  --force-takeover                   Allow adopting existing Wrist Agent-named resources after review.
  --non-interactive                  Do not ask for an absent initial Workspace token.
  --help                             Show this help.

Do not pass access tokens as command-line arguments. For a later redeploy,
omit --workspace-agent-token-file to reuse the current secret version.
USAGE
}

die() {
  printf 'Error: %s\n' "$*" >&2
  exit 64
}

note() {
  printf '%s\n' "$*" >&2
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "Required command is unavailable: $1"
}

random_urlsafe() {
  local bytes="$1"
  openssl rand -base64 "$bytes" | tr '+/' '-_' | tr -d '=\n'
}

validate_name() {
  local value="$1"
  local label="$2"
  [[ "$value" =~ ^[a-z]([a-z0-9-]{0,61}[a-z0-9])?$ ]] || die "$label must be a lowercase Google resource name"
}

validate_identifier() {
  local value="$1"
  local label="$2"
  [[ "$value" =~ ^[A-Za-z][A-Za-z0-9_-]{2,60}$ ]] || die "$label must start with a letter and contain 3-61 letters, digits, underscores, or hyphens"
}

validate_service_account_id() {
  local value="$1"
  [[ "$value" =~ ^[a-z][a-z0-9-]{4,28}[a-z0-9]$ ]] || die "--runtime-service-account-id must contain 6-30 lowercase letters, digits, or hyphens"
}

secret_exists() {
  gcloud secrets describe "$1" --project="$PROJECT_ID" >/dev/null 2>&1
}

verify_existing_secret_ownership() {
  local secret_name="$1"
  local labels
  if ! secret_exists "$secret_name"; then
    return
  fi
  labels="$(gcloud secrets describe "$secret_name" \
    --project="$PROJECT_ID" \
    --format='value(labels)')"
  if [[ "$labels" != *'managed-by=wrist-agent'* && "$FORCE_TAKEOVER" != true ]]; then
    die "An existing secret named $secret_name is not labelled managed-by=wrist-agent. Use --force-takeover only after reviewing it."
  fi
}

ensure_secret() {
  local secret_name="$1"
  if secret_exists "$secret_name"; then
    local labels
    labels="$(gcloud secrets describe "$secret_name" \
      --project="$PROJECT_ID" \
      --format='value(labels)')"
    if [[ "$labels" != *'managed-by=wrist-agent'* && "$FORCE_TAKEOVER" != true ]]; then
      die "An existing secret named $secret_name is not labelled managed-by=wrist-agent. Use --force-takeover only after reviewing it."
    fi
    if [[ "$labels" != *'managed-by=wrist-agent'* ]]; then
      note "Marking the reviewed secret $secret_name as managed by Wrist Agent"
      gcloud secrets update "$secret_name" \
        --project="$PROJECT_ID" \
        --update-labels='app=wrist-agent,managed-by=wrist-agent' \
        --quiet
    fi
    return
  fi
  note "Creating Secret Manager secret: $secret_name"
  gcloud secrets create "$secret_name" \
    --project="$PROJECT_ID" \
    --replication-policy=automatic \
    --labels='app=wrist-agent,managed-by=wrist-agent' \
    --quiet
}

latest_secret_version() {
  local version_path
  version_path="$(gcloud secrets versions list "$1" \
    --project="$PROJECT_ID" \
    --filter='state=ENABLED' \
    --sort-by='~createTime' \
    --limit=1 \
    --format='value(name)')"
  printf '%s\n' "${version_path##*/}"
}

add_secret_version() {
  local secret_name="$1"
  local secret_value="$2"
  local version_path
  version_path="$(printf %s "$secret_value" | gcloud secrets versions add "$secret_name" \
    --project="$PROJECT_ID" \
    --data-file=- \
    --quiet \
    --format='value(name)')"
  printf '%s\n' "${version_path##*/}"
}

grant_secret_access() {
  local secret_name="$1"
  gcloud secrets add-iam-policy-binding "$secret_name" \
    --project="$PROJECT_ID" \
    --member="serviceAccount:${RUNTIME_SERVICE_ACCOUNT}" \
    --role='roles/secretmanager.secretAccessor' \
    --quiet >/dev/null
}

ensure_runtime_service_account() {
  local description
  if ! gcloud iam service-accounts describe "$RUNTIME_SERVICE_ACCOUNT" \
    --project="$PROJECT_ID" >/dev/null 2>&1; then
    note "Creating the dedicated runtime service account"
    gcloud iam service-accounts create "$RUNTIME_SERVICE_ACCOUNT_ID" \
      --project="$PROJECT_ID" \
      --display-name='Wrist Agent Cloud Functions runtime' \
      --description='managed-by=wrist-agent; dedicated Cloud Functions runtime identity' \
      --quiet
    return
  fi

  description="$(gcloud iam service-accounts describe "$RUNTIME_SERVICE_ACCOUNT" \
    --project="$PROJECT_ID" \
    --format='value(description)')"
  if [[ "$description" != *'managed-by=wrist-agent'* && "$FORCE_TAKEOVER" != true ]]; then
    die "An existing runtime service account $RUNTIME_SERVICE_ACCOUNT is not marked managed-by=wrist-agent. Use --force-takeover only after reviewing its IAM roles."
  fi
  if [[ "$description" != *'managed-by=wrist-agent'* ]]; then
    note "Marking the reviewed runtime service account as managed by Wrist Agent"
    gcloud iam service-accounts update "$RUNTIME_SERVICE_ACCOUNT" \
      --project="$PROJECT_ID" \
      --description='managed-by=wrist-agent; dedicated Cloud Functions runtime identity' \
      --quiet
  fi
}

grant_project_role() {
  local member="$1"
  local role="$2"
  gcloud projects add-iam-policy-binding "$PROJECT_ID" \
    --member="$member" \
    --role="$role" \
    --quiet >/dev/null
}

grant_runtime_service_account_user() {
  local member="$1"
  gcloud iam service-accounts add-iam-policy-binding "$RUNTIME_SERVICE_ACCOUNT" \
    --project="$PROJECT_ID" \
    --member="$member" \
    --role='roles/iam.serviceAccountUser' \
    --quiet >/dev/null
}

ensure_firestore() {
  local database_type
  local database_location
  if gcloud firestore databases describe \
    --project="$PROJECT_ID" \
    --database='(default)' >/dev/null 2>&1; then
    database_type="$(gcloud firestore databases describe --project="$PROJECT_ID" --database='(default)' --format='value(type)')"
    database_location="$(gcloud firestore databases describe --project="$PROJECT_ID" --database='(default)' --format='value(locationId)')"
    case "$database_type" in
      FIRESTORE_NATIVE|firestore-native|'') ;;
      *) die "The existing default database is not Firestore Native: $database_type" ;;
    esac
    if [[ -n "$FIRESTORE_LOCATION" && "$database_location" != "$FIRESTORE_LOCATION" ]]; then
      die "The existing Firestore location is $database_location, not $FIRESTORE_LOCATION"
    fi
    note "Using existing Firestore Native database in $database_location"
    return
  fi

  [[ "$CREATE_FIRESTORE" == true ]] || die "Firestore does not exist. Re-run with --create-firestore after choosing its permanent location."
  [[ -n "$FIRESTORE_LOCATION" ]] || die "--firestore-location is required when creating Firestore"
  note "Creating the default Firestore Native database in $FIRESTORE_LOCATION"
  gcloud firestore databases create \
    --project="$PROJECT_ID" \
    --database='(default)' \
    --location="$FIRESTORE_LOCATION" \
    --edition=standard \
    --type=firestore-native \
    --delete-protection \
    --quiet
}

ensure_ttl() {
  local collection_group="$1"
  local configured_fields
  configured_fields="$(gcloud firestore fields ttls list \
    --project="$PROJECT_ID" \
    --database='(default)' \
    --collection-group="$collection_group" \
    --format='value(fieldPath)' 2>/dev/null || true)"
  if [[ "$configured_fields" == *purgeAt* ]]; then
    note "Firestore TTL is already configured for $collection_group"
    return
  fi

  note "Starting Firestore TTL configuration for $collection_group"
  gcloud firestore fields ttls update purgeAt \
    --project="$PROJECT_ID" \
    --database='(default)' \
    --collection-group="$collection_group" \
    --enable-ttl \
    --async \
    --quiet
}

resolve_workspace_token_version() {
  ensure_secret "$WORKSPACE_SECRET"
  local existing_version
  existing_version="$(latest_secret_version "$WORKSPACE_SECRET")"
  if [[ -n "$existing_version" && "$ROTATE_WORKSPACE_TOKEN" != true ]]; then
    if [[ -n "$WORKSPACE_TOKEN_VALUE" ]]; then
      die "The Workspace token secret already exists. Pass --rotate-workspace-token to replace it deliberately."
    fi
    WORKSPACE_SECRET_VERSION="$existing_version"
    return
  fi

  [[ -n "$WORKSPACE_TOKEN_VALUE" ]] || die "An initial Workspace Agent token is required via --workspace-agent-token-file, WORKSPACE_AGENT_ACCESS_TOKEN, or interactive input"
  WORKSPACE_SECRET_VERSION="$(add_secret_version "$WORKSPACE_SECRET" "$WORKSPACE_TOKEN_VALUE")"
  WORKSPACE_TOKEN_VALUE=""
}

resolve_generated_secret() {
  local secret_name="$1"
  local rotate="$2"
  local byte_count="$3"
  local reveal_existing="$4"
  ensure_secret "$secret_name"
  local existing_version
  existing_version="$(latest_secret_version "$secret_name")"
  if [[ -n "$existing_version" && "$rotate" != true ]]; then
    GENERATED_SECRET_VERSION="$existing_version"
    if [[ "$reveal_existing" == true ]]; then
      GENERATED_SECRET_VALUE="$(gcloud secrets versions access "$existing_version" \
        --secret="$secret_name" \
        --project="$PROJECT_ID")"
    else
      GENERATED_SECRET_VALUE=""
    fi
    return
  fi
  GENERATED_SECRET_VALUE="$(random_urlsafe "$byte_count")"
  GENERATED_SECRET_VERSION="$(add_secret_version "$secret_name" "$GENERATED_SECRET_VALUE")"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project) PROJECT_ID="${2:-}"; shift 2 ;;
    --region) REGION="${2:-}"; shift 2 ;;
    --firestore-location) FIRESTORE_LOCATION="${2:-}"; shift 2 ;;
    --function-name) FUNCTION_NAME="${2:-}"; shift 2 ;;
    --runtime-service-account-id) RUNTIME_SERVICE_ACCOUNT_ID="${2:-}"; shift 2 ;;
    --secret-prefix) SECRET_PREFIX="${2:-}"; shift 2 ;;
    --firestore-collection-prefix) COLLECTION_PREFIX="${2:-}"; shift 2 ;;
    --workspace-agent-trigger-id) WORKSPACE_AGENT_TRIGGER_ID="${2:-}"; shift 2 ;;
    --workspace-agent-token-file) WORKSPACE_TOKEN_FILE="${2:-}"; shift 2 ;;
    --max-instances) MAX_INSTANCES="${2:-}"; shift 2 ;;
    --concurrency) CONCURRENCY="${2:-}"; shift 2 ;;
    --function-timeout) FUNCTION_TIMEOUT="${2:-}"; shift 2 ;;
    --create-firestore) CREATE_FIRESTORE=true; shift ;;
    --force-takeover) FORCE_TAKEOVER=true; shift ;;
    --non-interactive) NON_INTERACTIVE=true; shift ;;
    --rotate-workspace-token) ROTATE_WORKSPACE_TOKEN=true; shift ;;
    --rotate-device-token) ROTATE_DEVICE_TOKEN=true; shift ;;
    --rotate-callback-pepper) ROTATE_CALLBACK_PEPPER=true; shift ;;
    --print-device-token) PRINT_DEVICE_TOKEN=true; shift ;;
    --device-token-output-file) DEVICE_TOKEN_OUTPUT_FILE="${2:-}"; shift 2 ;;
    --help|-h) usage; exit 0 ;;
    *) die "Unknown option: $1" ;;
  esac
done

require_command gcloud
require_command curl
require_command openssl
require_command tr

[[ -n "$PROJECT_ID" ]] || die "--project is required"
[[ -n "$REGION" ]] || die "--region is required"
[[ "$WORKSPACE_AGENT_TRIGGER_ID" =~ ^agtch_[A-Za-z0-9_-]+$ ]] || die "--workspace-agent-trigger-id must start with agtch_"
validate_name "$FUNCTION_NAME" '--function-name'
validate_service_account_id "$RUNTIME_SERVICE_ACCOUNT_ID"
validate_identifier "$COLLECTION_PREFIX" '--firestore-collection-prefix'
[[ "$MAX_INSTANCES" =~ ^[1-9][0-9]*$ ]] || die "--max-instances must be a positive integer"
[[ "$CONCURRENCY" =~ ^[1-9][0-9]*$ ]] || die "--concurrency must be a positive integer"
[[ "$FUNCTION_TIMEOUT" =~ ^[1-9][0-9]*[smh]$ ]] || die "--function-timeout must look like 120s, 2m, or 1h"
if [[ "$PRINT_DEVICE_TOKEN" == true && ! -t 1 ]]; then
  die "--print-device-token requires an interactive terminal; use --device-token-output-file in CI"
fi
if [[ -n "$DEVICE_TOKEN_OUTPUT_FILE" ]]; then
  [[ "$DEVICE_TOKEN_OUTPUT_FILE" != '-' ]] || die "--device-token-output-file must be a real file path"
  [[ ! -L "$DEVICE_TOKEN_OUTPUT_FILE" ]] || die "--device-token-output-file must not be a symlink"
  [[ -d "$(dirname -- "$DEVICE_TOKEN_OUTPUT_FILE")" ]] || die "The device token output directory does not exist"
fi

if [[ -z "$SECRET_PREFIX" ]]; then
  SECRET_PREFIX="$FUNCTION_NAME"
fi
validate_name "$SECRET_PREFIX" '--secret-prefix'

WORKSPACE_TOKEN_VALUE=""
if [[ -n "$WORKSPACE_TOKEN_FILE" ]]; then
  [[ -f "$WORKSPACE_TOKEN_FILE" ]] || die "Workspace token file does not exist: $WORKSPACE_TOKEN_FILE"
  [[ ! -L "$WORKSPACE_TOKEN_FILE" ]] || die "Workspace token file must not be a symlink"
  WORKSPACE_TOKEN_VALUE="$(<"$WORKSPACE_TOKEN_FILE")"
elif [[ -n "${WORKSPACE_AGENT_ACCESS_TOKEN:-}" ]]; then
  WORKSPACE_TOKEN_VALUE="$WORKSPACE_AGENT_ACCESS_TOKEN"
fi

WORKSPACE_SECRET="${SECRET_PREFIX}-workspace-token"
DEVICE_SECRET="${SECRET_PREFIX}-device-token"
PEPPER_SECRET="${SECRET_PREFIX}-callback-pepper"
RUNTIME_SERVICE_ACCOUNT="${RUNTIME_SERVICE_ACCOUNT_ID}@${PROJECT_ID}.iam.gserviceaccount.com"
PUBLIC_BASE_URL="https://${REGION}-${PROJECT_ID}.cloudfunctions.net/${FUNCTION_NAME}"

gcloud projects describe "$PROJECT_ID" --format='value(projectNumber)' >/dev/null
gcloud auth print-access-token >/dev/null

EXISTING_LABELS=""
if EXISTING_LABELS="$(gcloud functions describe "$FUNCTION_NAME" \
  --v2 \
  --project="$PROJECT_ID" \
  --region="$REGION" \
  --format='value(labels)' 2>/dev/null)"; then
  if [[ "$EXISTING_LABELS" != *'managed-by=wrist-agent'* && "$FORCE_TAKEOVER" != true ]]; then
    die "An existing function named $FUNCTION_NAME is not labelled managed-by=wrist-agent. Use --force-takeover only after reviewing it."
  fi
fi

note "Enabling required Google Cloud APIs"
gcloud services enable \
  artifactregistry.googleapis.com \
  cloudbuild.googleapis.com \
  cloudfunctions.googleapis.com \
  compute.googleapis.com \
  firestore.googleapis.com \
  iam.googleapis.com \
  logging.googleapis.com \
  run.googleapis.com \
  secretmanager.googleapis.com \
  --project="$PROJECT_ID" \
  --quiet

if [[ -z "$WORKSPACE_TOKEN_VALUE" ]] && ! secret_exists "$WORKSPACE_SECRET"; then
  if [[ "$NON_INTERACTIVE" == true ]]; then
    die "An initial Workspace Agent token is required via --workspace-agent-token-file or WORKSPACE_AGENT_ACCESS_TOKEN"
  fi
  read -r -s -p 'Workspace Agent access token (input hidden): ' WORKSPACE_TOKEN_VALUE
  printf '\n' >&2
fi

ensure_runtime_service_account
verify_existing_secret_ownership "$WORKSPACE_SECRET"
verify_existing_secret_ownership "$DEVICE_SECRET"
verify_existing_secret_ownership "$PEPPER_SECRET"
ensure_firestore

PROJECT_NUMBER="$(gcloud projects describe "$PROJECT_ID" --format='value(projectNumber)')"
BUILD_SERVICE_ACCOUNT="$(gcloud builds get-default-service-account \
  --project="$PROJECT_ID" \
  --region="$REGION" \
  --format='value(serviceAccountEmail)' 2>/dev/null || true)"
if [[ -z "$BUILD_SERVICE_ACCOUNT" ]]; then
  BUILD_SERVICE_ACCOUNT="${PROJECT_NUMBER}-compute@developer.gserviceaccount.com"
fi

note "Granting the runtime service account its narrow storage role"
grant_project_role "serviceAccount:${RUNTIME_SERVICE_ACCOUNT}" 'roles/datastore.user'
note "Ensuring the selected Cloud Build identity can build the function source"
grant_project_role "serviceAccount:${BUILD_SERVICE_ACCOUNT}" 'roles/run.builder'
grant_project_role "serviceAccount:${BUILD_SERVICE_ACCOUNT}" 'roles/cloudbuild.builds.builder'
note "Allowing the selected Cloud Build identity to attach the dedicated runtime service account"
grant_runtime_service_account_user "serviceAccount:${BUILD_SERVICE_ACCOUNT}"

WORKSPACE_SECRET_VERSION=""
resolve_workspace_token_version

GENERATED_SECRET_VALUE=""
GENERATED_SECRET_VERSION=""
REVEAL_DEVICE_TOKEN=false
if [[ "$PRINT_DEVICE_TOKEN" == true || -n "$DEVICE_TOKEN_OUTPUT_FILE" ]]; then
  REVEAL_DEVICE_TOKEN=true
fi
resolve_generated_secret "$DEVICE_SECRET" "$ROTATE_DEVICE_TOKEN" 32 "$REVEAL_DEVICE_TOKEN"
DEVICE_TOKEN_VALUE="$GENERATED_SECRET_VALUE"
DEVICE_SECRET_VERSION="$GENERATED_SECRET_VERSION"
resolve_generated_secret "$PEPPER_SECRET" "$ROTATE_CALLBACK_PEPPER" 48 false
PEPPER_SECRET_VERSION="$GENERATED_SECRET_VERSION"
GENERATED_SECRET_VALUE=""

grant_secret_access "$WORKSPACE_SECRET"
grant_secret_access "$DEVICE_SECRET"
grant_secret_access "$PEPPER_SECRET"

ensure_ttl "${COLLECTION_PREFIX}_requests"
ensure_ttl "${COLLECTION_PREFIX}_idempotency"

ENVIRONMENT_VARIABLES="PUBLIC_BASE_URL=${PUBLIC_BASE_URL},WORKSPACE_AGENT_TRIGGER_ID=${WORKSPACE_AGENT_TRIGGER_ID},FIRESTORE_COLLECTION_PREFIX=${COLLECTION_PREFIX},REQUEST_TTL_SECONDS=900,RETENTION_SECONDS=86400,WORKSPACE_AGENT_TIMEOUT_MS=15000,WORKSPACE_AGENT_MAX_ATTEMPTS=3,RATE_LIMIT_PER_MINUTE=20,MCP_RATE_LIMIT_PER_MINUTE=120,NODE_ENV=production"
SECRET_VARIABLES="WORKSPACE_AGENT_ACCESS_TOKEN=${WORKSPACE_SECRET}:${WORKSPACE_SECRET_VERSION},WRIST_AGENT_DEVICE_TOKENS=${DEVICE_SECRET}:${DEVICE_SECRET_VERSION},CALLBACK_CAPABILITY_PEPPER=${PEPPER_SECRET}:${PEPPER_SECRET_VERSION}"

deploy_once() {
  gcloud functions deploy "$FUNCTION_NAME" \
    --gen2 \
    --project="$PROJECT_ID" \
    --region="$REGION" \
    --runtime=nodejs22 \
    --source="$SOURCE_DIR" \
    --entry-point=wristAgentBridge \
    --trigger-http \
    --allow-unauthenticated \
    --ingress-settings=all \
    --service-account="$RUNTIME_SERVICE_ACCOUNT" \
    --memory=512Mi \
    --timeout="$FUNCTION_TIMEOUT" \
    --max-instances="$MAX_INSTANCES" \
    --concurrency="$CONCURRENCY" \
    --update-env-vars="$ENVIRONMENT_VARIABLES" \
    --update-secrets="$SECRET_VARIABLES" \
    --update-labels='app=wrist-agent,managed-by=wrist-agent' \
    --quiet
}

deploy_function() {
  local attempt=1
  local exit_status
  local output
  while (( attempt <= 3 )); do
    if output="$(deploy_once 2>&1)"; then
      printf '%s\n' "$output"
      return
    fi
    exit_status=$?
    printf '%s\n' "$output" >&2
    if (( attempt == 3 )) || [[ "$output" != *'PERMISSION_DENIED'* && "$output" != *'PermissionDenied'* && "$output" != *'Permission '* && "$output" != *'permission'* ]]; then
      return "$exit_status"
    fi
    note "Deployment authorization may still be propagating; retrying in 30 seconds (${attempt}/3)"
    sleep 30
    ((attempt += 1))
  done
}

note "Deploying Cloud Functions Gen 2 function $FUNCTION_NAME"
deploy_function

DEPLOYED_FUNCTION_URL="$(gcloud functions describe "$FUNCTION_NAME" \
  --v2 \
  --project="$PROJECT_ID" \
  --region="$REGION" \
  --format='value(url)')"
if [[ -z "$DEPLOYED_FUNCTION_URL" ]]; then
  DEPLOYED_FUNCTION_URL="$PUBLIC_BASE_URL"
fi
RUN_SERVICE_URL="$(gcloud functions describe "$FUNCTION_NAME" \
  --v2 \
  --project="$PROJECT_ID" \
  --region="$REGION" \
  --format='value(serviceConfig.uri)')"

for attempt in 1 2 3 4 5 6; do
  if curl --fail --silent --show-error "${PUBLIC_BASE_URL}/healthz" >/dev/null; then
    break
  fi
  if [[ "$attempt" == 6 ]]; then
    die "The function deployed, but ${PUBLIC_BASE_URL}/healthz did not become ready. Inspect Cloud Logging before retrying."
  fi
  sleep "$attempt"
done
curl --fail --silent --show-error "${PUBLIC_BASE_URL}/readyz" >/dev/null

printf '\nDeployment complete.\n'
printf 'BRIDGE_URL=%s\n' "$PUBLIC_BASE_URL"
printf 'MCP_URL=%s/mcp\n' "$PUBLIC_BASE_URL"
printf 'FUNCTION_URL=%s\n' "$DEPLOYED_FUNCTION_URL"
printf 'RUN_SERVICE_URL=%s\n' "$RUN_SERVICE_URL"
if [[ -n "$DEVICE_TOKEN_OUTPUT_FILE" ]]; then
  (umask 077; printf '%s\n' "$DEVICE_TOKEN_VALUE" > "$DEVICE_TOKEN_OUTPUT_FILE")
  chmod 600 "$DEVICE_TOKEN_OUTPUT_FILE"
  printf 'DEVICE_TOKEN=written to %s (mode 0600)\n' "$DEVICE_TOKEN_OUTPUT_FILE"
elif [[ "$PRINT_DEVICE_TOKEN" == true ]]; then
  printf 'DEVICE_TOKEN=%s\n' "$DEVICE_TOKEN_VALUE"
else
  printf 'DEVICE_TOKEN=not printed (use --print-device-token in an interactive terminal or --device-token-output-file in CI)\n'
fi
printf '\nNext: paste MCP_URL into a private ChatGPT MCP connection, attach it to the Workspace Agent, republish the API channel, then put BRIDGE_URL and the securely retrieved device token into the Pebble settings.\n'
