import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import ts from "typescript";

const source = await readFile(new URL("../src/workbench-forms.ts", import.meta.url), "utf8");
const compiled = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext } }).outputText;
const { WorkbenchForms } = await import(`data:text/javascript;base64,${Buffer.from(compiled).toString("base64")}`);

const panel = (id) => ({ id, generation: "20", kind: "fields", form: "person", formGeneration: "10",
  draftRevision: "1", resultRevision: "0", visible: true, focused: false });
function scenario() {
  let available = true, next = 0, views;
  const sent = [], focused = [], a = panel("a"), b = panel("b");
  const forms = new WorkbenchForms({ send(message) { sent.push(message); }, nextId: () => `s.${++next}`,
    available: () => available, changed(value) { views = value; }, returning() {},
    focus(pane) { const id = `s.${++next}`; focused.push({ id, pane }); return id; } });
  forms.observe([a, b]);
  forms.page({ form: "person", generation: "10", draftRevision: "1", resultRevision: "0", section: "fields",
    total: 1, offset: 0, nextOffset: null, records: [{ id: "name", generation: "11", access: "edit", value: "Guest" }] });
  return { forms, a, b, sent, focused, view: () => views["person:10"], available(value) { available = value; } };
}
for (const transition of ["hidden", "removed", "replaced"]) {
  const run = scenario();
  run.available(false); run.forms.change(run.a, "name", "local draft", false);
  run.forms.observe(transition === "removed" ? [run.b]
    : [{ ...run.a, visible: transition !== "hidden", generation: transition === "replaced" ? "21" : "20" }, run.b]);
  run.available(true);
  for (let i = 0; i < 10; i++) run.forms.result({ requestId: "unrelated", state: "complete", code: "ok" });
  assert.equal(run.focused.length, 0, `${transition} queued pane never requests focus`);
  assert.equal(run.sent.some((request) => request.operation === "form.set"), false);
  assert.match(run.view().error, /pane.*unavailable/);
  assert.equal(run.view().values.name, "local draft", "unadmitted local draft remains available for reconciliation");
}
{
  const run = scenario();
  run.forms.change(run.a, "name", "local draft", false);
  assert.equal(run.focused.length, 1);
  run.forms.result({ requestId: run.focused[0].id, state: "complete", code: "busy-modal", message: "Modal input retains ownership" });
  for (let i = 0; i < 10; i++) run.forms.result({ requestId: "unrelated", state: "complete", code: "ok" });
  assert.equal(run.focused.length, 1, "refused focus never retries automatically");
  assert.equal(run.sent.some((request) => request.operation === "form.set"), false);
  assert.match(run.view().error, /Modal/);
}
console.log("PASS form coordinator: hidden/removed/replaced sibling panes and refused focus retain drafts without retry loops");
