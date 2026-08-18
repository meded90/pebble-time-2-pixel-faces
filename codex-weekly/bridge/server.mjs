import { createServer, get } from "node:http";
import { spawn, spawnSync } from "node:child_process";
import { createInterface } from "node:readline";
import { timingSafeEqual } from "node:crypto";

import { readT3RawCost } from "./t3-raw-cost.mjs";

const host = process.env.CODEX_PEBBLE_HOST || "127.0.0.1";
const port = Number(process.env.CODEX_PEBBLE_PORT || 8765);
const accessToken = process.env.CODEX_PEBBLE_TOKEN || "";
const codexBinary = process.env.CODEX_BIN || "codex";
const cacheDurationMs = Number(process.env.CODEX_PEBBLE_CACHE_MS || 120000);
const networkInterface = process.env.CODEX_PEBBLE_INTERFACE || "en0";

function isIpv4Address(value) {
  const octets = value.split(".");
  return octets.length === 4 && octets.every((octet) => {
    const number = Number(octet);
    return Number.isInteger(number) && number >= 0 && number <= 255;
  });
}

function readInterfaceAddress(interfaceName) {
  const result = spawnSync(
    process.env.IPCONFIG_BIN || "/usr/sbin/ipconfig",
    ["getifaddr", interfaceName],
    { encoding: "utf8" },
  );
  const address = result.status === 0 ? result.stdout.trim() : "";
  return isIpv4Address(address) ? address : null;
}

function checkHealth(url) {
  return new Promise((resolve) => {
    const request = get(url, (response) => {
      response.resume();
      resolve(response.statusCode === 200);
    });
    request.setTimeout(2000, () => request.destroy());
    request.on("error", () => resolve(false));
  });
}

if (host !== "127.0.0.1" && host !== "::1" && !accessToken) {
  throw new Error(
    "CODEX_PEBBLE_TOKEN is required when listening outside localhost.",
  );
}

let appServer = null;
let appServerReady = null;
let nextRequestId = 1;
let pendingRequests = new Map();
let statusCache = null;

function writeMessage(message) {
  if (!appServer || !appServer.stdin.writable) {
    throw new Error("Codex app-server is not running.");
  }
  appServer.stdin.write(`${JSON.stringify(message)}\n`);
}

function rejectPending(error) {
  for (const pending of pendingRequests.values()) {
    clearTimeout(pending.timeout);
    pending.reject(error);
  }
  pendingRequests.clear();
}

function requestRaw(method, params = {}) {
  const id = nextRequestId++;
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      pendingRequests.delete(id);
      reject(new Error(`Codex app-server request timed out: ${method}`));
    }, 15000);

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
          pending.reject(
            new Error(message.error.message || "Codex app-server error"),
          );
        } else {
          pending.resolve(message.result);
        }
        return;
      }

      if (message.method && message.id !== undefined) {
        writeMessage({
          id: message.id,
          error: {
            code: -32601,
            message: `Unsupported bridge client method: ${message.method}`,
          },
        });
      }
    });

    requestRaw("initialize", {
      clientInfo: {
        name: "pebble_codex_weekly",
        title: "Pebble Codex Weekly Bridge",
        version: "1.0.3",
      },
    }).then(() => {
      writeMessage({ method: "initialized", params: {} });
      resolve();
    }, (error) => {
      reject(error);
    });
  });

  return appServerReady;
}

async function codexRequest(method, params = {}) {
  await startAppServer();
  return requestRaw(method, params);
}

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

function selectWeeklyWindow(rateLimitResult) {
  const buckets = rateLimitResult.rateLimitsByLimitId
    ? Object.entries(rateLimitResult.rateLimitsByLimitId).map(
      ([limitId, bucket]) => ({ ...bucket, limitId: bucket.limitId || limitId }),
    )
    : [rateLimitResult.rateLimits]
      .filter(Boolean)
      .map((bucket) => ({ ...bucket, limitId: bucket.limitId || "codex" }));
  const windows = [];

  for (const bucket of buckets) {
    for (const windowName of ["primary", "secondary"]) {
      const window = bucket?.[windowName];
      if (
        window &&
        Number.isFinite(Number(window.usedPercent)) &&
        Number.isFinite(Number(window.windowDurationMins))
      ) {
        windows.push({
          limitId: bucket.limitId || "codex",
          limitName: bucket.limitName || null,
          windowName,
          usedPercent: Number(window.usedPercent),
          windowDurationMins: Number(window.windowDurationMins),
          resetsAt: Number(window.resetsAt) || 0,
        });
      }
    }
  }

  const mainWindows = windows.filter((window) => window.limitId === "codex");
  const dailyOrLonger = mainWindows.filter(
    (window) => window.windowDurationMins >= 24 * 60,
  );
  const candidates = dailyOrLonger.length ? dailyOrLonger : mainWindows;
  candidates.sort(
    (left, right) => right.windowDurationMins - left.windowDurationMins,
  );

  console.log(
    "[codex-weekly] available rate-limit windows",
    JSON.stringify(windows),
  );

  const weekly = candidates[0];
  if (!weekly) {
    throw new Error("Codex did not return the main quota window.");
  }

  const selected = {
    ...weekly,
    usedPercent: clamp(Math.round(weekly.usedPercent), 0, 100),
    leftPercent: clamp(Math.round(100 - weekly.usedPercent), 0, 100),
  };
  console.log(
    "[codex-weekly] selected weekly window",
    JSON.stringify({
      ...selected,
      rawUsedPercent: weekly.usedPercent,
      rawLeftPercent: 100 - weekly.usedPercent,
    }),
  );
  return selected;
}

function utcDate(date) {
  return new Date(
    Date.UTC(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate()),
  );
}

function buildActivity(dailyUsageBuckets) {
  const levels = new Array(84).fill(0);
  const tokens = new Array(84).fill(0);
  const today = utcDate(new Date());
  const mondayOffset = (today.getUTCDay() + 6) % 7;
  const currentMonday = new Date(today);
  currentMonday.setUTCDate(currentMonday.getUTCDate() - mondayOffset);
  const start = new Date(currentMonday);
  start.setUTCDate(start.getUTCDate() - 11 * 7);

  for (const bucket of dailyUsageBuckets || []) {
    if (!bucket?.startDate) {
      continue;
    }
    const date = new Date(`${bucket.startDate}T00:00:00Z`);
    const dayIndex = Math.round((date - start) / 86400000);
    if (dayIndex >= 0 && dayIndex < 84) {
      tokens[dayIndex] = Math.max(0, Number(bucket.tokens) || 0);
    }
  }

  const nonZero = tokens.filter((value) => value > 0).sort((a, b) => a - b);
  const threshold = (fraction) => {
    if (!nonZero.length) {
      return 0;
    }
    return nonZero[Math.min(
      nonZero.length - 1,
      Math.floor((nonZero.length - 1) * fraction),
    )];
  };
  const thresholds = [threshold(0.25), threshold(0.5), threshold(0.75)];

  for (let index = 0; index < tokens.length; index += 1) {
    if (tokens[index] === 0) {
      levels[index] = 0;
    } else if (tokens[index] <= thresholds[0]) {
      levels[index] = 1;
    } else if (tokens[index] <= thresholds[1]) {
      levels[index] = 2;
    } else if (tokens[index] <= thresholds[2]) {
      levels[index] = 3;
    } else {
      levels[index] = 4;
    }
  }

  return {
    startDate: start.toISOString().slice(0, 10),
    levels: levels.join(""),
    dailyUsageBuckets: dailyUsageBuckets || [],
  };
}

async function readStatus() {
  if (statusCache && statusCache.expiresAt > Date.now()) {
    console.log(
      "[codex-weekly] status response",
      JSON.stringify({
        source: "cache",
        generatedAt: statusCache.payload.generatedAt,
        weekly: statusCache.payload.weekly,
      }),
    );
    return statusCache.payload;
  }

  const [rateLimits, usage] = await Promise.all([
    codexRequest("account/rateLimits/read"),
    codexRequest("account/usage/read"),
  ]);
  const weekly = selectWeeklyWindow(rateLimits);
  const periodEndMs = Date.now();
  const periodStartMs =
    weekly.resetsAt * 1000 - weekly.windowDurationMins * 60 * 1000;
  try {
    const rawCost = await readT3RawCost({
      startMs: periodStartMs,
      endMs: periodEndMs,
    });
    weekly.rawCostUsd = rawCost.rawCostUsd;
    console.log(
      "[codex-weekly] T3 Code raw cost",
      JSON.stringify({
        rawCostUsd: rawCost.rawCostUsd,
        pricedRecords: rawCost.pricedRecords,
        unpricedRecords: rawCost.unpricedRecords,
      }),
    );
  } catch (error) {
    console.warn(
      "[codex-weekly] T3 Code raw cost unavailable",
      error instanceof Error ? error.message : String(error),
    );
  }

  const payload = {
    generatedAt: new Date().toISOString(),
    weekly,
    activity: {
      ...buildActivity(usage.dailyUsageBuckets),
      summary: usage.summary || null,
    },
  };

  statusCache = {
    payload,
    expiresAt: Date.now() + cacheDurationMs,
  };
  console.log(
    "[codex-weekly] status response",
    JSON.stringify({
      source: "fresh",
      generatedAt: payload.generatedAt,
      weekly: payload.weekly,
    }),
  );
  return payload;
}

function tokenMatches(request) {
  if (!accessToken) {
    return true;
  }
  const header = request.headers.authorization || "";
  const supplied = header.startsWith("Bearer ") ? header.slice(7) : "";
  const expectedBuffer = Buffer.from(accessToken);
  const suppliedBuffer = Buffer.from(supplied);
  return (
    expectedBuffer.length === suppliedBuffer.length &&
    timingSafeEqual(expectedBuffer, suppliedBuffer)
  );
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

const server = createServer(async (request, response) => {
  if (request.method === "OPTIONS") {
    sendJson(response, 204, {});
    return;
  }
  if (request.url === "/healthz") {
    sendJson(response, 200, { ok: true });
    return;
  }
  if (request.method !== "GET" || request.url?.split("?")[0] !== "/status") {
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
    console.error(error);
    sendJson(response, 503, {
      error: error instanceof Error ? error.message : "Codex bridge failed",
    });
  }
});

server.listen(port, host, async () => {
  const serverAddress = server.address();
  const actualPort = typeof serverAddress === "object" && serverAddress
    ? serverAddress.port
    : port;
  const interfaceAddress = readInterfaceAddress(networkInterface);
  const isWildcardHost = host === "0.0.0.0" || host === "::";
  const advertisedHost = isWildcardHost ? interfaceAddress : host;

  if (interfaceAddress) {
    console.log(
      `Network: ipconfig getifaddr ${networkInterface} -> ${interfaceAddress}`,
    );
  } else {
    console.warn(
      `Network: ipconfig getifaddr ${networkInterface} did not return an IPv4 address.`,
    );
  }

  if (!advertisedHost) {
    console.log(`Local URL: http://127.0.0.1:${actualPort}/status`);
    console.warn(
      `Set CODEX_PEBBLE_INTERFACE to the active interface and restart the bridge.`,
    );
    return;
  }

  const statusUrl = `http://${advertisedHost}:${actualPort}/status`;
  const healthUrl = `http://${advertisedHost}:${actualPort}/healthz`;
  console.log(`Status URL: ${statusUrl}`);

  if (!isWildcardHost && (host === "127.0.0.1" || host === "::1")) {
    console.warn(
      "The bridge is listening on localhost only. Use CODEX_PEBBLE_HOST=0.0.0.0 for access from the phone.",
    );
  }

  const healthy = await checkHealth(healthUrl);
  console.log(`Health check: ${healthy ? "OK" : "FAILED"} (${healthUrl})`);
});

function shutdown() {
  server.close();
  if (appServer) {
    appServer.kill();
  }
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
