import assert from "node:assert/strict";
import { parseRunResult } from "../vm/run-host.mjs";
const envelope = (changes) =>
  "LLB1\n" +
  Object.entries({
    indicator: "R",
    hasStatus: "1",
    status: "9223372036854775807",
    hasValue: "1",
    valueHex: "CEB1",
    hasPrimary: "0",
    primaryNameHex: "",
    primaryMessageHex: "",
    primaryCode: "0",
    primaryModule: "0",
    primaryAddress: "0",
    hasCleanup: "0",
    cleanupNameHex: "",
    cleanupMessageHex: "",
    cleanupCode: "0",
    cleanupModule: "0",
    cleanupAddress: "0",
    truncated: "0",
    ...changes,
  })
    .map(([key, value]) => `${key}=${value}\n`)
    .join("");
let result = parseRunResult(envelope());
assert.equal(
  result.status,
  "9223372036854775807",
  "status must retain full integer precision",
);
assert.equal(result.value, "α");
assert.equal(parseRunResult(envelope({valueHex:'61'.repeat(1048576)})).value.length,1048576);
result = parseRunResult(
  envelope({
    indicator: "E",
    hasStatus: "0",
    status: "0",
    hasValue: "0",
    valueHex: "",
    hasPrimary: "1",
    primaryNameHex: "4552524F52",
    primaryMessageHex: "626164",
  }),
);
assert.equal(
  result.status,
  undefined,
  "failed result zero must not become successful status",
);
assert.equal(result.primary.name, "ERROR");
assert.equal(result.primary.message, "bad");
assert.equal(
  parseRunResult(envelope({ truncated: "1", valueHex: "CE" })).value,
  "�",
);
for (const wire of [
  envelope() + "status=2\n",
  envelope({ hasStatus: "2" }),
  envelope({ valueHex: "AA0" }),
  envelope({ status: "1.5" }),
  envelope().slice(0, -1),
  "LLB0\n",
])
  assert.throws(() => parseRunResult(wire));
console.log(
  "PASS interpreter control envelope: exact status, typed failure, Unicode/truncation, duplicate/malformed refusal",
);
