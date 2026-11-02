import assert from "node:assert/strict";
import { checkEditorCommands } from "./command-policy.mjs";

export async function wasmEditorCommandsSmoke({send, snapshot, evaluate, command, waitFor, workerSession}) {
  const request = async (message, predicate = () => true) => {
    const before = await evaluate("window.__messages.filter(m => m.screen_rows).length");
    await send(message);
    return waitFor(async () => {
      if (!await evaluate(`window.__messages.filter(m => m.screen_rows).length > ${before}`)) return false;
      const view = await snapshot();
      return predicate(view) ? view : false;
    }, JSON.stringify(message));
  };
  const editorCommand = text => request({type: "command", command: text});
  const typedCommand = async text => {
    await editorCommand("cursor cmdline 1");
    await request({type: "action", action: "edit.undo"}, s => s.command === "");
    await request({type: "text", text}, s => s.command === text);
    return request({type: "key", key: "enter"});
  };
  const savedBytes = path => evaluate(`new Promise((resolve,reject) => {
    const open = indexedDB.open('/workspace');
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const db = open.result;
      const get = db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
      get.onsuccess = () => { resolve(get.result ? Array.from(get.result.contents) : null); db.close(); };
      get.onerror = () => { reject(get.error); db.close(); };
    };
  })`);

  await checkEditorCommands(request);
  const original = await snapshot();
  const path = original.buffer.path;
  assert.equal(original.buffers.length, 1, "last-file contract starts with one buffer");
  const durable = await savedBytes(path);
  await editorCommand("cursor goto 1 1");
  await editorCommand("set insertmode on");
  await request({type: "text", text: "DISCARD "}, s => s.buffer.dirty);
  const beforeFailure = await evaluate("window.__messages.length");
  await command("Runtime.evaluate", {expression: `globalThis.commandTestPut = IDBObjectStore.prototype.put;
    IDBObjectStore.prototype.put = function() { throw new DOMException('command test quota refusal', 'QuotaExceededError'); };`}, workerSession);
  try {
    await typedCommand("save");
    await waitFor(() => evaluate(`window.__messages.slice(${beforeFailure}).some(m => m.type === 'storage' && m.state === 'error')`), "typed SAVE storage refusal");
    assert.equal((await snapshot()).buffer.dirty, 1, "failed typed SAVE retains unsaved edits");
    assert.deepEqual(await savedBytes(path), durable, "failed typed SAVE preserves durable bytes");
  } finally {
    await command("Runtime.evaluate", {expression: "IDBObjectStore.prototype.put = globalThis.commandTestPut"}, workerSession);
  }

  // Restore the test fixture after the injected failure, then make a fresh
  // unsaved edit for the independent discard/last-buffer contract.
  await editorCommand("cursor goto 1 1");
  const firstLine = original.screen_rows.find(row => row.role === "file" && row.line === 1).t;
  await editorCommand(`replace ${firstLine}`);
  await typedCommand("save");
  await waitFor(async () => !(await snapshot()).buffer.dirty, "typed SAVE retry");
  assert.deepEqual(await savedBytes(path), durable, "typed SAVE retry restores the saved fixture");
  await editorCommand("cursor goto 1 1");
  await request({type: "text", text: "DISCARD "}, s => s.buffer.dirty);

  await typedCommand("qquit");
  let view = await waitFor(async () => {
    const current = await snapshot();
    return current.buffer.path !== path && current.buffers.length === 1 && current;
  }, "QQUIT last file leaves a new buffer");
  const firstBlank = view.buffer.path;
  assert.match(firstBlank, /\/Untitled-\d+\.txt$/);
  assert.equal(view.buffer.lines, 0);
  assert.equal(view.buffer.dirty, 0);
  assert.equal(view.buffers.length, 1);
  assert.deepEqual(await savedBytes(path), durable, "last-file QQUIT must not save discarded text");
  assert.equal(await savedBytes(firstBlank), null, "fresh blank buffer remains unsaved");
  assert.ok(!await evaluate("window.__messages.some(m => m.type === 'ended')"), "QQUIT must keep the editor Worker alive");

  await editorCommand("input usable blank buffer");
  await typedCommand("save");
  await waitFor(async () => !(await snapshot()).buffer.dirty, "typed SAVE in replacement buffer");
  assert.equal(Buffer.from(await savedBytes(firstBlank)).toString("utf8"), "usable blank buffer\n");
  view = await request({type: "action", action: "file.close"}, s => s.buffers.length === 1 && s.buffer.path !== firstBlank);
  const secondBlank = view.buffer.path;
  assert.notEqual(secondBlank, firstBlank, "closing the last clean buffer uses a fresh path");
  assert.equal(view.buffer.lines, 0);
  assert.equal(await savedBytes(secondBlank), null);
  assert.equal(Buffer.from(await savedBytes(firstBlank)).toString("utf8"), "usable blank buffer\n");

  await request({type: "action", action: "file.open", argument: path});
  await request({type: "action", action: "buffer.switch", argument: secondBlank});
  view = await editorCommand("qquit");
  assert.equal(view.buffer.path, path);
  assert.equal(view.buffer.dirty, 0);
  assert.equal(view.buffers.length, 1);
  assert.ok(!await evaluate("window.__messages.some(m => m.type === 'ended')"));
  console.log("PASS browser editor commands: viewport scroll, safe edits, typed SAVE durability/refusal and usable QQUIT last-file close");
}
