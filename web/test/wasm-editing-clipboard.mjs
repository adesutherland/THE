import assert from "node:assert/strict";
import { mkdir, writeFile } from "node:fs/promises";
import { dirname } from "node:path";

export async function editingClipboardSmoke({ send, snapshot, evaluate, command, waitFor }) {
  const path = "/workspace/Clipboard acceptance.txt";
  const text = Array.from({ length: 1300 }, (_, index) => `Line ${index}: Zoë 東京 🙂 ${"x".repeat(70)}`).join("\n");
  const menu = async (group, action) => {
    await evaluate("document.querySelector('.input-sink').focus();document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'F10',bubbles:true,cancelable:true}))");
    await evaluate(`[...document.querySelectorAll('.menu-groups button')].find(n=>n.textContent===${JSON.stringify(group)}).click()`);
    await evaluate(`document.querySelector(${JSON.stringify(`[data-action-id="${action}"]`)}).click()`);
  };
  const selectAll = async () => {
    const count = await evaluate("window.__messages.filter(m=>m.type==='clipboard'&&m.operation==='selectAll').length");
    await evaluate("document.querySelector('.input-sink').focus();document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'a',ctrlKey:true,bubbles:true,cancelable:true}))");
    await waitFor(() => evaluate(`window.__messages.filter(m=>m.type==='clipboard'&&m.operation==='selectAll').length>${count}`), "Select All core result");
    assert.equal(await evaluate("window.__messages.filter(m=>m.type==='clipboard'&&m.operation==='selectAll').at(-1).rc"), 0, "Select All is admitted for the actual file target");
    await waitFor(() => evaluate("window.__snapshot.selection?.active && document.querySelectorAll('.logical-selection').length>0"), "authoritative source selection is visible");
    assert.equal(await evaluate("[...document.querySelectorAll('.logical-selection')].every(node=>getComputedStyle(node).pointerEvents==='none' && getComputedStyle(node).backgroundColor!=='rgba(0, 0, 0, 0)' && node.getBoundingClientRect().width>0)"), true, "selection overlay is visible without owning native input");
  };
  const paste = async (value) => {
    const priorRevision = (await snapshot()).source.revision;
    const count = await evaluate("window.__messages.filter(m=>m.type==='clipboard'&&m.operation==='paste').length");
    await evaluate(`(() => {const input=document.querySelector('.input-sink');input.focus();const data=new DataTransfer();data.setData('text/plain',${JSON.stringify(value)});input.dispatchEvent(new ClipboardEvent('paste',{bubbles:true,cancelable:true,clipboardData:data}));})()`);
    await waitFor(() => evaluate(`window.__messages.filter(m=>m.type==='clipboard'&&m.operation==='paste').length>${count}`), "atomic paste result");
    const result = await evaluate("window.__messages.filter(m=>m.type==='clipboard'&&m.operation==='paste').at(-1)");
    assert.equal(result.rc, 0, JSON.stringify(result));
    try {
      await waitFor(async () => (await snapshot()).source.revision !== priorRevision, "paste semantic snapshot after receipt");
      await waitFor(() => evaluate("document.querySelectorAll('.logical-selection').length===0"), "Paste clears authoritative selection highlight");
    } catch (error) {
      await send({ type: "snapshot" });
      await evaluate("new Promise(resolve=>setTimeout(resolve,200))");
      console.error("Clipboard diagnostic after explicit snapshot", JSON.stringify(await snapshot()));
      const beforeSave = await evaluate("window.__messages.filter(m=>m.type==='storage'&&m.state==='saved').length");
      await send({ type: "action", action: "file.save" });
      await waitFor(() => evaluate(`window.__messages.filter(m=>m.type==='storage'&&m.state==='saved').length>${beforeSave}`), "diagnostic save after missing paste snapshot", 3000);
      const bytes = await savedBytes();
      console.error("Clipboard diagnostic durable bytes", JSON.stringify({ length: bytes?.length, expectedLength: Buffer.byteLength(value + "\n"), exact: bytes && Buffer.from(bytes).toString("utf8") === value + "\n" }));
      throw error;
    }
  };
  const copied = async () => {
    await evaluate("window.clipboardWritten=null");
    await menu("Edit", "the.edit.copy");
    return waitFor(() => evaluate("window.clipboardWritten"), "complete editor clipboard export");
  };
  const savedBytes = () => evaluate(`new Promise((resolve,reject) => {
    const open=indexedDB.open('/workspace');open.onerror=()=>reject(open.error);
    open.onsuccess=()=>{const db=open.result;const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
      get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close()};get.onerror=()=>{reject(get.error);db.close()};};})`);
  await command("Emulation.setDeviceMetricsOverride", { width: 820, height: 720, deviceScaleFactor: 1, mobile: false });
  await evaluate("Object.defineProperty(navigator,'clipboard',{configurable:true,value:{writeText:async text=>{window.clipboardWritten=text},readText:async()=>window.clipboardText||''}})");
  await menu("File", "the.file.create");
  await waitFor(() => evaluate("document.activeElement?.id==='file-path'"), "New path field");
  await evaluate(`(() => {const input=document.querySelector('#file-path');input.value=${JSON.stringify(path)};input.dispatchEvent(new Event('input',{bubbles:true}));})()`);
  await waitFor(() => evaluate("!document.querySelector('.path-dialog button[type=submit]').disabled"), "New path accepted");
  await evaluate("document.querySelector('.path-dialog').requestSubmit()");
  await waitFor(async () => (await snapshot()).buffer.path === path, "New editable file");
  const blank = await snapshot();
  assert.equal(blank.buffer.lines, 0); assert.equal(blank.buffer.dirty, 0);
  await evaluate("document.querySelector('.input-sink').focus()");
  await command("Input.insertText", { text: "🙂" });
  await waitFor(async () => (await snapshot()).screen_rows.some(row => row.role === "file" && row.t === "🙂"), "first text in New file");
  await send({ type: "action", action: "edit.undo" });
  await waitFor(async () => (await snapshot()).buffer.lines === 0, "Undo restores New blank file");
  assert.equal((await snapshot()).buffer.dirty, 0);
  await paste("Saved seed 🙂");
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty && !!await savedBytes(), "seed saved");
  const seed = await snapshot();
  await selectAll();
  await paste(text);
  assert.equal((await snapshot()).buffer.lines, 1300);
  assert.equal((await snapshot()).buffer.dirty, 1);
  await selectAll();
  assert.equal(await copied(), text, "large Unicode copy includes complete pending text");
  await send({ type: "action", action: "edit.undo" });
  await waitFor(async () => (await snapshot()).buffer.lines === 1, "one Undo restores complete selection replacement");
  let view = await snapshot();
  assert.equal(view.screen_rows.find(row => row.role === "file")?.t, "Saved seed 🙂");
  assert.equal(view.buffer.dirty, 0, "Undo restores prior dirty state");
  assert.deepEqual([view.focus.line, view.focus.cell], [seed.focus.line, seed.focus.cell], "Undo restores prior cursor");
  await selectAll(); await paste(text);
  await send({ type: "command", command: "cursor goto 1 1" });
  await waitFor(async () => (await snapshot()).focus.line === 1, "top of large selection for visual review");
  await selectAll();
  assert.equal(await evaluate("document.querySelectorAll('.logical-selection').length===window.__snapshot.screen_rows.filter(row=>row.role==='file').length"), true, "Select All visibly covers every displayed selected file row");
  if (process.env.THE_WASM_SELECTION_SCREENSHOT) {
    await mkdir(dirname(process.env.THE_WASM_SELECTION_SCREENSHOT), { recursive: true });
    const screenshot = await command("Page.captureScreenshot", { format: "png" });
    await writeFile(process.env.THE_WASM_SELECTION_SCREENSHOT, Buffer.from(screenshot.data, "base64"));
    console.log("Source selection screenshot: " + process.env.THE_WASM_SELECTION_SCREENSHOT);
  }

  await evaluate("navigator.clipboard.writeText=async()=>{throw new DOMException('Clipboard permission denied','NotAllowedError')}");
  await menu("Edit", "the.edit.cut");
  await waitFor(() => evaluate("document.querySelector('.interaction-notice').textContent.includes('permission denied')"), "denied Cut feedback");
  assert.equal((await snapshot()).buffer.lines, 1300, "denied Cut preserves bytes");
  await evaluate("navigator.clipboard.writeText=async text=>{window.clipboardWritten=text}");
  await selectAll(); assert.equal(await copied(), text, "denied Cut retains full source");
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "large paste saved");
  assert.equal(Buffer.from(await savedBytes()).toString("utf8"), text + "\n", "persisted bytes match complete paste");
  await send({ type: "action", action: "file.close" });
  await waitFor(async () => (await snapshot()).buffer.path !== path, "close saved clipboard file");
  await send({ type: "action", action: "file.open", argument: path });
  await waitFor(async () => (await snapshot()).buffer.path === path, "reopen saved clipboard file");
  await evaluate("document.querySelector('.role-file .line-content').click()");
  await waitFor(async () => (await snapshot()).focus.zone === "filearea", "reopened file logical focus");
  await selectAll(); assert.equal(await copied(), text, "reopened bytes retain large Unicode paste");
  console.log("PASS browser editing: New/first input/Undo, 1300-line Unicode selection replacement in one Undo, pending copy, denied Cut, Save/reopen bytes at 820x720");
}
