#!/usr/bin/env sh
set -eu

: "${CODEX_AUTH_FILE:=/var/run/secrets/codex/auth.json}"

if [ ! -r "$CODEX_AUTH_FILE" ]; then
  echo "Codex auth secret is not mounted at $CODEX_AUTH_FILE" >&2
  exit 1
fi

umask 077
# Secret Manager mounts are atomically replaced by Cloud Run. Stream the
# current content instead of using install(1), which rejects such a file.
cat "$CODEX_AUTH_FILE" > /root/.codex/auth.json
chmod 600 /root/.codex/auth.json
exec node /app/server.mjs
