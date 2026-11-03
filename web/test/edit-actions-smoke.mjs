import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import ts from "typescript";
const source = await readFile(new URL("../src/edit-actions.ts", import.meta.url), "utf8");
const compiled = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext } }).outputText;
const { performEdit, validateClipboardText } = await import(`data:text/javascript;base64,${Buffer.from(compiled).toString("base64")}`);
const target = { kind: "editor", element: {} };
const selected = "Zoë\n東京🙂\n".repeat(3000);
{
  const calls = [];
  await assert.rejects(performEdit("cut", target, async (operation) => {
    calls.push(operation); return { rc: 0, token: "9", text: selected };
  }, { writeText: async () => { throw new Error("permission denied"); } }), /permission denied/);
  assert.deepEqual(calls, ["copy"], "denied clipboard write never deletes editor selection");
}
{
  const calls = [];
  let copied = "";
  await performEdit("cut", target, async (operation, values) => {
    calls.push([operation, values]);
    if (operation === "cut") assert.equal(copied, selected, "clipboard is written before cut mutation");
    return { rc: 0, token: "19", text: selected };
  }, { writeText: async (text) => { copied = text; } });
  assert.deepEqual(calls, [["copy", undefined], ["cut", { token: "19" }]]);
}
{
  const calls = [];
  await assert.rejects(performEdit("cut", target, async (operation) => {
    calls.push(operation);
    if (operation === "cut") throw new Error("stale revision");
    return { rc: 0, token: "20", text: selected };
  }, { writeText: async () => {} }), /stale revision/);
  assert.deepEqual(calls, ["copy", "cut"], "stale cut is refused without retrying against new editor state");
}
{
  const calls = [];
  await performEdit("paste", target, async (operation, values) => {
    calls.push([operation, values]); return { rc: 0, token: "21" };
  }, { readText: async () => selected });
  assert.deepEqual(calls, [["capture", undefined], ["paste", { token: "21", text: selected }]], "large Unicode multiline paste uses one complete core transaction");
}
{
  const calls = [];
  await assert.rejects(performEdit("paste", target, async (operation) => { calls.push(operation); return { token: "22" }; },
    { readText: async () => { throw new Error("permission denied"); } }), /permission denied/);
  assert.deepEqual(calls, ["capture"], "denied clipboard read does not request a mutation");
  const emptyCalls = [];
  await assert.rejects(performEdit("paste", target, async (operation) => { emptyCalls.push(operation); return { token: "23" }; },
    { readText: async () => "" }), /contains no text/);
  assert.deepEqual(emptyCalls, ["capture"], "empty clipboard preserves the selected text");
  assert.throws(() => validateClipboardText("🙂".repeat(262145)), /1 MiB/);
  assert.throws(() => validateClipboardText("a\0b"), /NUL/);
}
console.log("PASS clipboard coordinator: permission refusals, clipboard-before-cut, stale cut, large Unicode and atomic paste token");
