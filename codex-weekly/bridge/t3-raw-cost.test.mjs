import assert from "node:assert/strict";
import test from "node:test";

import { calculateT3RawCost } from "./t3-raw-cost.mjs";

test("matches T3 Code pricing for the selected Codex period", () => {
  const scanCache = {
    version: 2,
    models: ["openai/gpt-priced", "codex-auto-review"],
    sessions: ["session"],
    files: {
      codex: {
        p: "codex",
        r: [
          [2000, 0, 0, 2_000_000, 8_000_000, 1_000_000, 500_000, 0, null, null],
          [2500, 0, 0, 0, 0, 0, 0, 0, null, 7.25],
          [2600, 1, 0, 10_000_000, 0, 0, 0, 0, null, null],
          [999, 0, 0, 99_000_000, 0, 0, 0, 0, null, null],
        ],
      },
      claude: {
        p: "claude",
        r: [[2200, 0, 0, 99_000_000, 0, 0, 0, 0, null, null]],
      },
    },
  };
  const ratesDocument = {
    "gpt-priced": {
      input_cost_per_token: 1.25e-6,
      output_cost_per_token: 10e-6,
      cache_read_input_token_cost: 0.125e-6,
      cache_creation_input_token_cost: 1.25e-6,
    },
  };

  const result = calculateT3RawCost({
    scanCache,
    ratesDocument,
    startMs: 1000,
    endMs: 3000,
  });

  assert.equal(result.rawCostUsd, 17);
  assert.equal(result.pricedRecords, 2);
  assert.equal(result.unpricedRecords, 1);
});

test("rejects an incompatible T3 Code scan cache", () => {
  assert.throws(
    () => calculateT3RawCost({
      scanCache: { version: 1 },
      ratesDocument: {},
      startMs: 1000,
      endMs: 2000,
    }),
    /unavailable or incompatible/,
  );
});
