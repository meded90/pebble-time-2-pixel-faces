FROM node:22-bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends ffmpeg \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY package.json ./
COPY src ./src
RUN mkdir -p /data && chown node:node /data

ENV PORT=8787 DATA_DIR=/data
VOLUME ["/data"]
EXPOSE 8787
USER node
CMD ["node", "src/server.js"]
