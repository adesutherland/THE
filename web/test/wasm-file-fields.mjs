import assert from "node:assert/strict";

// Runs inside the existing Chromium/Wasm harness. All setup uses public editor
// and profile-build requests; source bytes are inspected through DOCUMENT/IDBFS.
export async function fileFieldSmoke({ send, evaluate, command, waitFor, snapshot }) {
  await command("Emulation.setDeviceMetricsOverride", {width: 1200, height: 900, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  const panelId = "user.wasmfields.pane", rawId = "user.wasmfields.raw";
  const source = `!pName: !uGuest   !p END ${"protected ".repeat(12)}\n!pOther: !uReady   !p END\n`;
  async function editorCommand(text) {
    const before = await evaluate("window.__messages.filter(m=>m.screen_rows).length");
    await send({ type: "command", command: text });
    await waitFor(() => evaluate(`window.__messages.filter(m=>m.screen_rows).length>${before}`), text);
  }
  const state = (id) => evaluate(`window.__messages.findLast(m=>m.operation==='panel-state')?.panels.find(p=>p.id===${JSON.stringify(id)})`);
  async function durable() {
    return evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');open.onerror=()=>reject(open.error);open.onsuccess=()=>{
      const db=open.result,get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/workspace/FileFields.txt');
      get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};get.onerror=()=>reject(get.error);};})`);
  }
  async function rawBytes() {
    const panel = await state(rawId), before = await evaluate("window.__messages.length");
    await send({ type: "workbench", version: 1, operation: "document.list", panel: rawId,
      expectGeneration: panel.generation, expectRevision: panel.documentRevision, line: 1, offset: 0, records: 8, bytes: 8192 });
    const page = await waitFor(() => evaluate(`window.__messages.slice(${before}).find(m=>m.operation==='document'&&m.data.panel===${JSON.stringify(rawId)})?.data`), "Wasm raw source inspection");
    return page.records.map((record) => Buffer.from(record.bytesHex, "hex").toString("utf8")).join("\n") + "\n";
  }
  const fieldValue = () => evaluate("document.querySelector('.file-field-editable input')?.value.trimEnd()");
  async function set(value) {
    await evaluate("(() => {const input=document.querySelector('.file-field-editable input');input.focus();input.select();})()");
    await command("Input.insertText", { text: value });
    await waitFor(async () => await fieldValue() === value && await evaluate("!document.querySelector('.file-field-editable input').disabled"), `Wasm SET ${value}`);
  }
  async function apply() {
    const before = await evaluate("window.__messages.length");
    await evaluate("document.querySelector('.file-field-controls button').click()");
    const receipt = await waitFor(() => evaluate(`window.__messages.slice(${before}).find(m=>m.operation==='result'&&m.state==='complete'&&m.group&&m.group!=='0')`), "Wasm Apply group receipt");
    assert.equal(receipt.code, "ok");
    await waitFor(() => evaluate("!document.querySelector('.file-field-editable input').disabled"), "Wasm Apply page admission");
    return receipt;
  }
  await send({ type: "action", action: "file.create", argument: "/workspace/FileFields.txt" });
  await waitFor(async () => (await snapshot()).buffer?.path === "/workspace/FileFields.txt", "Wasm fixture buffer");
  for (const line of source.trimEnd().split("\n")) await editorCommand("input " + line);
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty && await durable() !== null, "Wasm fixture Save");
  assert.equal(Buffer.from(await durable()).toString("utf8"), source);

  await send({ type: "tool", action: "profile" });
  await waitFor(async () => (await snapshot()).buffer?.path.endsWith("/user.profile.the"), "Wasm profile source");
  await editorCommand("cursor goto 4 1");
  await editorCommand('replace \'edit "/workspace/FileFields.txt"\';');
  const setup = ["set ctlchar ! escape", "set ctlchar p protect white", "set ctlchar u noprotect green", "set filectlchar on"];
  for (const [index, text] of setup.entries()) {
    await editorCommand(`cursor goto ${index + 4} 1`);
    await editorCommand(`input '${text}';`);
  }
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "Wasm profile saved");
  const previousBuild = await evaluate("window.__tools.build?.job.id ?? 0");
  const runtimeBeforeBuild = await evaluate("JSON.stringify(window.__tools.job)");
  await send({ type: "tool", action: "build" });
  await waitFor(() => evaluate(`window.__tools?.build?.job.id>${previousBuild}&&window.__tools.build.job.tool==='profile-compiler'&&!window.__tools.build.job.running`), "Wasm field profile compilation", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0");
  assert.equal(await evaluate("JSON.stringify(window.__tools.job)"), runtimeBeforeBuild, "profile compilation preserves the Program job");
  await send({ type: "tool", action: "activate" });
  await waitFor(async () => (await snapshot()).last_message === "Profile activated", "Wasm configuration profile activation", 30000);
  // User profiles are intentionally untrusted for workbench definitions. The
  // existing browser command path defines the pane after activation completes.
  for (const text of ["workbench begin owner user.wasmfields",
    `workbench panel put ${panelId} kind fields buffer active markup ctlchar access staged`,
    `workbench panel place ${panelId} right fallback below,tab mincols 10`,
    `workbench panel put ${rawId} kind document buffer active access readonly`, "workbench commit"]) await editorCommand(text);
  await waitFor(() => state(panelId), "Wasm core file-field capture");
  await evaluate(`document.querySelector('.named-file-fields-toggle[data-panel-id="${panelId}"]').click()`);
  await waitFor(() => evaluate("document.querySelectorAll('.file-field-row').length===2"), "Wasm copied file-field rows");
  assert.equal(await evaluate("document.querySelectorAll('.file-field-protected input').length"), 0);
  await evaluate(`document.querySelector('.named-file-fields-toggle[data-panel-id="${panelId}"]').click()`);
  await waitFor(() => evaluate("document.activeElement?.dataset.fieldId && !document.activeElement.disabled"), "Wasm file-field focus");
  await evaluate("document.querySelector('.file-field-editable input').setSelectionRange(1,1)");
  await command("Input.insertText", { text: "Z" });
  await waitFor(async () => await fieldValue() === "GZest" && await evaluate("document.querySelector('.file-field-editable input').selectionStart===2"), "Wasm core EDIT overwrite and UTF16 caret");
  await set("界😀");
  assert.equal(await rawBytes(), source, "SET/EDIT leave logical source unchanged");
  const receipt = await apply(), applied = source.replace("!uGuest   !p", "!u界😀    !p");
  assert.equal(await rawBytes(), applied, "Apply preserves exact raw markers and other records");
  assert.equal(Buffer.from(await durable()).toString("utf8"), source, "Apply keeps Save explicit");
  const appliedState = await state(panelId);
  await editorCommand(`workbench edit undo ${receipt.group} document ${appliedState.documentId} expect ${appliedState.writeRevision}`);
  assert.equal(await rawBytes(), source, "conditional group undo restores complete raw source");
  await evaluate("document.querySelector('.file-field-controls button:nth-child(4)').click()");
  await waitFor(async () => await fieldValue() === "Guest" && await evaluate("!document.querySelector('.file-field-editable input').disabled"), "Wasm Reload after undo");
  await set("界😀"); await apply();
  await command("Emulation.setDeviceMetricsOverride", { width: 390, height: 800, deviceScaleFactor: 1, mobile: false });
  await waitFor(async () => (await state(panelId)).effectivePlacement === "tab", "Wasm phone placement");
  assert.equal(await evaluate("(() => {const grid=document.querySelector('.file-field-grid');return grid.scrollWidth>grid.clientWidth&&document.documentElement.scrollWidth<=innerWidth})()"), true);
  await evaluate("document.querySelector('.file-field-controls button:nth-child(3)').click()");
  await waitFor(async () => !(await state(panelId)).focused, "Wasm Return");
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty && Buffer.from(await durable()).toString("utf8") === applied, "Wasm exact durable workspace bytes");
  console.log("PASS real Wasm file-field browser: core capture, protected rows, core EDIT/caret, Unicode SET, atomic Apply/raw bytes, conditional group undo, explicit durable Save and phone layout");
}
