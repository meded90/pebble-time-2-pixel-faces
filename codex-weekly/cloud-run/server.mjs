import { createServer } from "node:http";
import { spawn } from "node:child_process";
import { createInterface } from "node:readline";
import { timingSafeEqual } from "node:crypto";

const port = Number(process.env.PORT || 8080);
const accessToken = process.env.CODEX_WEEKLY_CLIENT_TOKEN || "";
const codexBinary = process.env.CODEX_BIN || "codex";
const cacheDurationMs = Number(process.env.CODEX_WEEKLY_CACHE_MS || 120000);
const requestTimeoutMs = Number(process.env.CODEX_WEEKLY_RPC_TIMEOUT_MS || 15000);

if (!accessToken) {
  throw new Error("CODEX_WEEKLY_CLIENT_TOKEN is required.");
}

let appServer = null;
let appServerReady = null;
let nextRequestId = 1;
const pendingRequests = new Map();
let statusCache = null;

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

function sendJson(response, status, body) {
  response.writeHead(status, {
    "Access-Control-Allow-Headers": "Authorization, Content-Type",
    "Access-Control-Allow-Methods": "GET, OPTIONS",
    "Access-Control-Allow-Origin": "*",
    "Cache-Control": "no-store",
    "Content-Type": "application/json; charset=utf-8",
  });
  response.end(JSON.stringify(body));
}

function tokenMatches(request) {
  const header = request.headers.authorization || "";
  const supplied = header.startsWith("Bearer ") ? header.slice(7) : "";
  const expectedBuffer = Buffer.from(accessToken);
  const suppliedBuffer = Buffer.from(supplied);
  return expectedBuffer.length === suppliedBuffer.length &&
    timingSafeEqual(expectedBuffer, suppliedBuffer);
}

function rejectPending(error) {
  for (const pending of pendingRequests.values()) {
    clearTimeout(pending.timeout);
    pending.reject(error);
  }
  pendingRequests.clear();
}

function writeMessage(message) {
  if (!appServer?.stdin.writable) {
    throw new Error("Codex app-server is not running.");
  }
  appServer.stdin.write(`${JSON.stringify(message)}\n`);
}

function requestRaw(method, params = {}) {
  const id = nextRequestId++;
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      pendingRequests.delete(id);
      reject(new Error(`Codex app-server request timed out: ${method}`));
    }, requestTimeoutMs);

    pendingRequests.set(id, { resolve, reject, timeout });
    writeMessage({ method, id, params });
  });
}

function startAppServer() {
  if (appServerReady) {
    return appServerReady;
  }

  appServerReady = new Promise((resolve, reject) => {
    appServer = spawn(codexBinary, ["app-server"], {
      stdio: ["pipe", "pipe", "inherit"],
    });

    appServer.once("error", (error) => {
      appServer = null;
      appServerReady = null;
      rejectPending(error);
      reject(error);
    });

    appServer.once("exit", (code, signal) => {
      const error = new Error(
        `Codex app-server exited (${code ?? signal ?? "unknown"}).`,
      );
      appServer = null;
      appServerReady = null;
      rejectPending(error);
    });

    const lines = createInterface({ input: appServer.stdout });
    lines.on("line", (line) => {
      let message;
      try {
        message = JSON.parse(line);
      } catch {
        return;
      }

      if (message.id !== undefined && pendingRequests.has(message.id)) {
        const pending = pendingRequests.get(message.id);
        pendingRequests.delete(message.id);
        clearTimeout(pending.timeout);
        if (message.error) {
          pending.reject(new Error(message.error.message || "Codex app-server error"));
        } else {
          pending.resolve(message.result);
        }
        return;
      }

      if (message.method && message.id !== undefined) {
        writeMessage({
          id: message.id,
          error: { code: -32601, message: "Unsupported client method" },
        });
      }
    });

    requestRaw("initialize", {
      clientInfo: {
        name: "pebble_codex_weekly_cloud_run",
        title: "Pebble Codex Weekly Cloud Run",
        version: "0.1.0",
      },
    }).then(() => {
      writeMessage({ method: "initialized", params: {} });
      resolve();
    }, reject);
  });

  return appServerReady;
}

async function codexRequest(method) {
  await startAppServer();
  return requestRaw(method);
}

function selectMainWindow(rateLimitResult) {
  const buckets = rateLimitResult.rateLimitsByLimitId
    ? Object.entries(rateLimitResult.rateLimitsByLimitId).map(
      ([limitId, bucket]) => ({ ...bucket, limitId: bucket.limitId || limitId }),
    )
    : [rateLimitResult.rateLimits]
      .filter(Boolean)
      .map((bucket) => ({ ...bucket, limitId: bucket.limitId || "codex" }));

  const windows = [];
  for (const bucket of buckets) {
    if (bucket.limitId !== "codex") {
      continue;
    }
    for (const windowName of ["primary", "secondary"]) {
      const window = bucket[windowName];
      if (window && Number.isFinite(Number(window.usedPercent)) &&
          Number.isFinite(Number(window.windowDurationMins))) {
        windows.push({
          usedPercent: Number(window.usedPercent),
          windowDurationMins: Number(window.windowDurationMins),
          resetsAt: Number(window.resetsAt) || 0,
        });
      }
    }
  }

  const dayOrLonger = windows.filter((window) => window.windowDurationMins >= 1440);
  const candidates = dayOrLonger.length ? dayOrLonger : windows;
  candidates.sort((left, right) => right.windowDurationMins - left.windowDurationMins);
  const selected = candidates[0];
  if (!selected) {
    throw new Error("Codex did not return the main quota window.");
  }

  return {
    usedPercent: clamp(Math.round(selected.usedPercent), 0, 100),
    leftPercent: clamp(Math.round(100 - selected.usedPercent), 0, 100),
    windowDurationMins: selected.windowDurationMins,
    resetsAt: selected.resetsAt,
  };
}

function classifyStatusError(error) {
  const message = error instanceof Error ? error.message.toLowerCase() : "";
  if (message.includes("token_expired") ||
      message.includes("invalid refresh token") ||
      message.includes("could not be refreshed") ||
      message.includes("401 unauthorized")) {
    return "AUTH";
  }
  if (message.includes("timed out") || message.includes("timeout")) {
    return "TIME";
  }
  if (message.includes("main quota window")) {
    return "DATA";
  }
  return "ERR";
}

async function readStatus() {
  if (statusCache?.expiresAt > Date.now()) {
    return statusCache.payload;
  }

  const rateLimits = await codexRequest("account/rateLimits/read");
  const payload = {
    generatedAt: new Date().toISOString(),
    weekly: selectMainWindow(rateLimits),
  };
  statusCache = { payload, expiresAt: Date.now() + cacheDurationMs };
  console.log("[codex-weekly] quota refreshed", JSON.stringify({
    generatedAt: payload.generatedAt,
    usedPercent: payload.weekly.usedPercent,
    windowDurationMins: payload.weekly.windowDurationMins,
  }));
  return payload;
}

const server = createServer(async (request, response) => {
  const pathname = new URL(request.url || "/", "http://localhost").pathname;
  if (request.method === "OPTIONS") {
    sendJson(response, 204, {});
    return;
  }
  // /healthz is intercepted by the Cloud Run frontend in this deployment.
  // Keep the public readiness endpoint on a non-reserved path.
  if (request.method === "GET" && pathname === "/health") {
    sendJson(response, 200, { ok: true });
    return;
  }
  if (request.method !== "GET" || pathname !== "/status") {
    sendJson(response, 404, { error: "Not found" });
    return;
  }
  if (!tokenMatches(request)) {
    sendJson(response, 401, { error: "Unauthorized" });
    return;
  }

  try {
    sendJson(response, 200, await readStatus());
  } catch (error) {
    console.error("[codex-weekly] status failed", error instanceof Error ? error.message : error);
    sendJson(response, 503, {
      error: "Quota temporarily unavailable",
      code: classifyStatusError(error),
    });
  }
});

function shutdown() {
  server.close();
  appServer?.kill();
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);

server.listen(port, "0.0.0.0", () => {
  console.log(`[codex-weekly] listening on ${port}`);
});
