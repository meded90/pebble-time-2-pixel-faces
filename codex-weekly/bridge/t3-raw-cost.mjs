import { readFile } from "node:fs/promises";
import { homedir } from "node:os";
import { join } from "node:path";

const T3_SCAN_CACHE_VERSION = 2;
const UNPRICEABLE_MODELS = new Set([
  "<synthetic>",
  "synthetic",
  "opus",
  "sonnet",
  "haiku",
  "fable",
]);

function finiteNumber(value) {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function normalizeModelName(model) {
  const normalized = model.trim().toLowerCase();
  const slash = normalized.lastIndexOf("/");
  return slash === -1 ? normalized : normalized.slice(slash + 1);
}

function parseRateTable(document) {
  const table = new Map();
  if (!document || typeof document !== "object") {
    return table;
  }

  for (const [name, raw] of Object.entries(document)) {
    if (!raw || typeof raw !== "object") {
      continue;
    }
    const input = finiteNumber(raw.input_cost_per_token);
    const output = finiteNumber(raw.output_cost_per_token);
    if (input === null || output === null) {
      continue;
    }
    table.set(normalizeModelName(name), {
      input,
      output,
      cacheRead: finiteNumber(raw.cache_read_input_token_cost) ?? input,
      cacheCreation:
        finiteNumber(raw.cache_creation_input_token_cost) ?? input,
    });
  }
  return table;
}

function lookupRate(table, model) {
  const normalized = normalizeModelName(model);
  if (!normalized || UNPRICEABLE_MODELS.has(normalized)) {
    return null;
  }
  return table.get(normalized) ?? null;
}

export function calculateT3RawCost({
  scanCache,
  ratesDocument,
  startMs,
  endMs,
}) {
  if (
    !scanCache ||
    typeof scanCache !== "object" ||
    scanCache.version !== T3_SCAN_CACHE_VERSION ||
    !Array.isArray(scanCache.models) ||
    !scanCache.models.every((model) => typeof model === "string") ||
    !scanCache.files ||
    typeof scanCache.files !== "object"
  ) {
    throw new Error("T3 Code usage scan cache is unavailable or incompatible.");
  }
  if (!Number.isFinite(startMs) || !Number.isFinite(endMs) || startMs > endMs) {
    throw new Error("Raw-cost period is invalid.");
  }

  const rates = parseRateTable(ratesDocument);
  if (!rates.size) {
    throw new Error("T3 Code model pricing cache is unavailable.");
  }

  let rawCostUsd = 0;
  let pricedRecords = 0;
  let unpricedRecords = 0;

  for (const file of Object.values(scanCache.files)) {
    if (!file || typeof file !== "object" || file.p !== "codex" ||
        !Array.isArray(file.r)) {
      continue;
    }

    for (const row of file.r) {
      if (!Array.isArray(row) || row.length < 10) {
        continue;
      }
      const timestampMs = finiteNumber(row[0]);
      if (timestampMs === null || timestampMs < startMs || timestampMs > endMs) {
        continue;
      }

      const model = Number.isInteger(row[1]) ? scanCache.models[row[1]] : null;
      if (typeof model !== "string") {
        continue;
      }

      const reportedCostUsd = finiteNumber(row[9]);
      if (reportedCostUsd !== null) {
        rawCostUsd += reportedCostUsd;
        pricedRecords += 1;
        continue;
      }

      const rate = lookupRate(rates, model);
      if (!rate) {
        unpricedRecords += 1;
        continue;
      }

      const uncachedInput = finiteNumber(row[3]);
      const cachedInput = finiteNumber(row[4]);
      const cacheCreation = finiteNumber(row[5]);
      const output = finiteNumber(row[6]);
      if (
        uncachedInput === null ||
        cachedInput === null ||
        cacheCreation === null ||
        output === null
      ) {
        continue;
      }

      rawCostUsd +=
        uncachedInput * rate.input +
        cachedInput * rate.cacheRead +
        cacheCreation * rate.cacheCreation +
        output * rate.output;
      pricedRecords += 1;
    }
  }

  return { rawCostUsd, pricedRecords, unpricedRecords };
}

export async function readT3RawCost({
  startMs,
  endMs,
  userDataDir = process.env.T3_USERDATA_DIR || join(homedir(), ".t3", "userdata"),
}) {
  const [scanCacheText, ratesCacheText] = await Promise.all([
    readFile(join(userDataDir, "usage-scan-cache.json"), "utf8"),
    readFile(join(userDataDir, "usage-model-rates.json"), "utf8"),
  ]);
  const scanCache = JSON.parse(scanCacheText);
  const ratesCache = JSON.parse(ratesCacheText);
  return calculateT3RawCost({
    scanCache,
    ratesDocument: ratesCache.document,
    startMs,
    endMs,
  });
}
