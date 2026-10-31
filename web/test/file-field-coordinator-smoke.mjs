import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import ts from "typescript";
const source = await readFile(new URL("../src/workbench-file-fields.ts", import.meta.url), "utf8");
const compiled = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext } }).outputText;
const { WorkbenchFileFields } = await import(`data:text/javascript;base64,${Buffer.from(compiled).toString("base64")}`);
const initial = "9007199254740993";
function scenario() {
  const sent = [], focused = [], returns = []; let views, available = true, next = 0;
  let panel = { id: "user.fields", generation: initial, kind: "file-fields", visible: true, focused: true,
    documentId: initial, writeRevision: initial, draftRevision: initial, projectionActive: true, projectionDirty: false };
  const fields = new WorkbenchFileFields({ send: (message) => sent.push(message), nextId: () => `s.${++next}`,
    available: () => available, changed: (value) => { views = value; }, returning: (pane) => returns.push(pane),
    focus: (pane) => { const id = `s.${++next}`; focused.push({ id, pane }); return id; } });
  const page = (revision = panel.draftRevision, value = "Guest") => fields.page({ schemaVersion: 1, section: "file-fields",
    panel: panel.id, panelGeneration: panel.generation, documentId: panel.documentId, writeRevision: panel.writeRevision,
    draftRevision: revision, active: true, dirty: value !== "Guest", offset: 0, nextOffset: null, total: 1,
    records: [{ line: 1, cells: 80, spans: [{ id: "", cell: 0, maxcells: 10, value: "Protected", editable: false },
      { id: "s1", cell: 10, maxcells: 50, value, editable: true }] }] });
  fields.observe([panel]); page();
  return { fields, sent, focused, returns, panel: () => panel, page,
    view: () => views[`user.fields:${initial}`], available: (value) => { available = value; },
    observe: (change) => { panel = { ...panel, ...change }; fields.observe([panel]); },
    complete: (message, revision, value) => {
      fields.result({ requestId: message.requestId, state: "complete", code: "ok", draftRevision: revision });
      page(revision, value); panel = { ...panel, draftRevision: revision }; fields.observe([panel]);
    } };
}
{
  const r = scenario(); r.fields.change(r.panel(), "", "Cannot change protected", false);
  assert.equal(r.sent.some((message) => message.operation === "filefield.set"), false);
  r.fields.change(r.panel(), "s1", "A", false);
  r.fields.change(r.panel(), "s1", "Alice 😀", false); r.fields.intent(r.panel(), "apply");
  const first = r.sent.find((message) => message.operation === "filefield.set");
  assert.equal(first.expectRevision, initial); assert.equal(first.expectGeneration, initial);
  r.complete(first, "9007199254740994", "A");
  const sets = r.sent.filter((message) => message.operation === "filefield.set");
  assert.equal(sets.length, 2); assert.equal(sets[1].expectRevision, "9007199254740994");
  assert.equal(sets[1].value, "Alice 😀"); assert(!r.sent.some((message) => message.operation === "filefield.apply"));
  r.complete(sets[1], "9007199254740995", "Alice 😀");
  const apply = r.sent.find((message) => message.operation === "filefield.apply");
  assert.equal(apply.expectRevision, "9007199254740995");
}
{
  const r = scenario(); r.fields.change(r.panel(), "s1", "未完", true); r.fields.intent(r.panel(), "apply");
  assert(!r.sent.some((message) => message.operation === "filefield.apply" || message.operation === "filefield.set"));
  assert.equal(r.view().locked, true);
  r.fields.change(r.panel(), "s1", "完成 😀", false);
  const set = r.sent.find((message) => message.operation === "filefield.set"); assert.equal(set.value, "完成 😀");
  r.complete(set, "9007199254740994", "完成 😀");
  assert(r.sent.some((message) => message.operation === "filefield.apply"));
}
{
  const r = scenario(); r.fields.change(r.panel(), "s1", "retained conflict", true);
  r.observe({ draftRevision: "9007199254740994" }); r.page("9007199254740994", "external");
  r.fields.change(r.panel(), "s1", "retained conflict", false);
  assert.match(r.view().error, /stale-revision/); assert.equal(r.view().values.s1, "retained conflict");
  assert(!r.sent.some((message) => message.operation === "filefield.set"));
  r.fields.intent(r.panel(), "reload");
  const reload = r.sent.find((message) => message.operation === "filefield.reload");
  assert.equal(reload.expectRevision, "9007199254740994"); assert.equal(r.view().values.s1, "retained conflict");
  r.complete(reload, "9007199254740995", "external"); assert.deepEqual(r.view().values, {});
}
for (const transition of ["hidden", "replaced"]) {
  const r = scenario(); r.available(false); r.fields.change(r.panel(), "s1", "retained draft", false);
  r.observe(transition === "hidden" ? { visible: false } : { generation: "9007199254740994" });
  r.available(true); r.fields.result({ requestId: "unrelated", code: "ok" });
  assert(!r.sent.some((message) => message.operation === "filefield.set"));
  assert.equal(r.view().values.s1, "retained draft"); assert.match(r.view().error, /unavailable/);
}
{
  const r = scenario(); r.fields.change(r.panel(), "s1", "local", false);
  const set = r.sent.find((message) => message.operation === "filefield.set");
  r.fields.result({ requestId: set.requestId, code: "stale-revision", message: "buffer changed" });
  assert.match(r.view().error, /stale-revision/); assert.equal(r.view().values.s1, "local");
  r.fields.intent(r.panel(), "return"); assert.equal(r.returns.length, 1);
}
{
  const r = scenario();
  r.fields.edit(r.panel(), "s1", "insert", 1, 90);
  r.fields.edit(r.panel(), "s1", "insert", 1, 88);
  r.fields.intent(r.panel(), "apply");
  const first = r.sent.find((message) => message.operation === "filefield.edit");
  assert.equal(first.offset, 1); assert.equal(first.codepoint, 90);
  r.fields.result({ requestId: first.requestId, code: "ok", draftRevision: "9007199254740994", utf16Caret: 2 });
  r.page("9007199254740994", "GZest");
  const second = r.sent.filter((message) => message.operation === "filefield.edit")[1];
  assert.equal(second.offset, 2, "queued typing continues at core-admitted UTF16 caret");
  assert.equal(second.expectRevision, "9007199254740994");
  r.fields.result({ requestId: second.requestId, code: "ok", draftRevision: "9007199254740995", utf16Caret: 3 });
  r.page("9007199254740995", "GZXst");
  assert.equal(r.view().carets.s1.offset, 3); assert(r.sent.some((message) => message.operation === "filefield.apply"));
}
console.log("PASS file-field coordinator: exact tokens, serialized Apply, IME, protected spans, stale draft retention, explicit Reload and lifetime isolation");
