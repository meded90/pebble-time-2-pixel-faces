FROM node:22-slim

ENV NODE_ENV=production
WORKDIR /app

RUN apt-get update \
  && apt-get install --yes --no-install-recommends ca-certificates \
  && rm -rf /var/lib/apt/lists/*

# Keep this aligned with the CLI that produced the mounted auth.json. Updating
# Codex is an explicit migration because its internal auth storage is not a
# supported Cloud Run contract.
RUN npm install --global @openai/codex@0.146.1 \
  && mkdir --parents /root/.codex \
  && chmod 700 /root/.codex

COPY package.json server.mjs ./
COPY docker-entrypoint.sh /usr/local/bin/codex-weekly-entrypoint

RUN chmod 755 /usr/local/bin/codex-weekly-entrypoint

EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/codex-weekly-entrypoint"]
