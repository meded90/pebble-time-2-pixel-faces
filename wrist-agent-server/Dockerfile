FROM node:22.23.2-alpine AS dependencies
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci --omit=dev

FROM node:22.23.2-alpine
ENV NODE_ENV=production
WORKDIR /app
RUN addgroup -S wristagent && adduser -S -G wristagent wristagent
COPY --from=dependencies /app/node_modules ./node_modules
COPY package.json ./
COPY src ./src
RUN mkdir -p /app/data && chown -R wristagent:wristagent /app
USER wristagent
EXPOSE 8787
CMD ["node", "src/server.js"]
