import assert from "node:assert/strict";

// Drive the mounted inputs through CDP. In particular, a second submission
// must work without focusing the field again after the first one.
export async function inputFocusSmoke({manifest, send, snapshot, evaluate, command, waitFor, runCurrentFile}) {
  // Qualify automatic focus while Program input is visible. Compact layouts
  // exercise their explicit Expand/Return journey in the workbench fixture.
  await command("Emulation.setDeviceMetricsOverride", {width: 1200, height: 900, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  const active = id => evaluate(`document.activeElement?.id === ${JSON.stringify(id)}`);
  const key = async name => {
    await command("Input.dispatchKeyEvent", {type: "keyDown", key: name, code: name,
      windowsVirtualKeyCode: name === "Enter" ? 13 : name === "Escape" ? 27 : 117,
      ...(name === "Enter" ? {text: "\r"} : {})});
    await command("Input.dispatchKeyEvent", {type: "keyUp", key: name, code: name,
      windowsVirtualKeyCode: name === "Enter" ? 13 : name === "Escape" ? 27 : 117});
  };
  const click = async selector => {
    const point = await evaluate(`(() => { const r = document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();
      return {x: r.left + r.width / 2, y: r.top + r.height / 2}; })()`);
    for (const type of ["mousePressed", "mouseReleased"])
      await command("Input.dispatchMouseEvent", {type, x: point.x, y: point.y,
        button: "left", clickCount: 1});
  };
  const savedBytes = path => evaluate(`new Promise((resolve, reject) => {
    const open = indexedDB.open('/workspace');
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const db = open.result;
      const get = db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
      get.onsuccess = () => { resolve(get.result ? Array.from(get.result.contents) : null); db.close(); };
      get.onerror = () => { reject(get.error); db.close(); };
    };
  })`);
  const unchanged = (before, after, label) => {
    assert.deepEqual({path: after.buffer.path, lines: after.buffer.lines, dirty: after.buffer.dirty,
      source: after.source}, {path: before.buffer.path, lines: before.buffer.lines,
      dirty: before.buffer.dirty, source: before.source}, `${label} changed editor bytes or dirty state`);
  };
  const commandText = async (text, button = false) => {
    assert.equal(await active("the-command"), true, `command input lost focus before ${text}`);
    const beforeMessages = await evaluate("window.__messages.filter(m => m.screen_rows).length");
    await command("Input.insertText", {text});
    assert.equal(await evaluate("document.querySelector('#the-command').value"), text);
    if (button) await click('.command-form button[type="submit"]');
    else await key("Enter");
    await waitFor(() => evaluate(`window.__messages.filter(m => m.screen_rows).length > ${beforeMessages}`),
      `${text} editor response`);
    await waitFor(() => evaluate("document.querySelector('#the-command').value === ''"), `${text} field cleared`);
    assert.equal(await active("the-command"), true, `${text} did not retain command input focus`);
  };

  // Keep a pending edit so cursor movement and command errors cannot be
  // mistaken for a clean-buffer-only success.
  const initial = await snapshot();
  const durable = await savedBytes(initial.buffer.path);
  await send({type: "command", command: "cursor goto 1 1"});
  await send({type: "text", text: "FOCUS "});
  const before = await waitFor(async () => {
    const view = await snapshot();
    return view.buffer.dirty && view.screen_rows.some(row => row.role === "file" && row.line === 1 && row.t.startsWith("FOCUS ")) && view;
  }, "pending editor text for focus test");
  await evaluate("document.querySelector('#the-command').focus()");
  await commandText("BOTTOM");
  let view = await snapshot();
  unchanged(before, view, "BOTTOM");
  assert.ok(view.screen_rows.some(row => row.line === before.buffer.lines),
    "BOTTOM did not expose the end of the file");
  await commandText("TOP");
  view = await snapshot();
  unchanged(before, view, "TOP");
  assert.ok(view.screen_rows.some(row => row.line === 0 && row.t.includes("Top of File")),
    "TOP did not expose the top of file");
  const oldSerial = view.last_message_serial;
  await commandText("FOCUS_NO_SUCH_COMMAND");
  view = await snapshot();
  unchanged(before, view, "rejected command");
  assert.notEqual(view.last_message_serial, oldSerial, "bad command did not report an error");
  await commandText("BOTTOM", true);
  unchanged(before, await snapshot(), "Execute button");
  assert.deepEqual(await savedBytes(before.buffer.path), durable, "commands changed saved file bytes");
  await key("Escape");
  assert.equal(await evaluate("document.activeElement?.classList.contains('input-sink')"), true,
    "Escape did not leave command input");
  await key("F6");
  assert.equal(await active("the-command"), true, "F6 did not return to command input");
  await key("F6");
  assert.equal(await evaluate("document.activeElement?.classList.contains('input-sink')"), true,
    "F6 did not leave command input");

  if (!manifest.capabilities.classic_rexx_interpreter) {
    console.log("PASS browser input focus: successive THE commands, Execute, Escape/F6 and source preservation");
    return;
  }
  await send({type: "action", action: "file.create", argument: "InputFocusProbe.rexx"});
  await waitFor(async () => (await snapshot()).buffer.path.endsWith("/InputFocusProbe.rexx"), "focus program buffer");
  await send({type: "command", command: "input say 'READY'; parse pull first; say 'FIRST:' || first; parse pull second; say 'SECOND:' || second; exit 0"});
  await waitFor(async () => (await snapshot()).screen_rows.some(row => row.t.includes("pull second")), "two PULL source");
  await send({type: "action", action: "file.save"});
  const program = await waitFor(async () => { const view = await snapshot(); return !view.buffer.dirty && view; }, "saved focus program");
  const programBytes = await savedBytes(program.buffer.path);
  const enterInput = async text => {
    assert.equal(await active("rexx-input"), true, `console input lost focus before ${text}`);
    await command("Input.insertText", {text});
    await key("Enter");
  };
  await runCurrentFile();
  await waitFor(() => evaluate("document.querySelector('.run-state')?.textContent === 'Waiting for input'"), "first PULL", 30000);
  await waitFor(() => active("rexx-input"), "first console focus");
  await enterInput("Alpha");
  await waitFor(() => evaluate("document.querySelector('.console-output')?.textContent.includes('FIRST:Alpha') && document.querySelector('.run-state')?.textContent === 'Waiting for input'"), "second PULL", 30000);
  assert.equal(await active("rexx-input"), true, "first stdin/output transition lost console focus");
  await enterInput("Beta");
  await waitFor(() => evaluate("document.querySelector('.run-state')?.textContent === 'Completed · status 0'"), "two PULL completion", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output')?.textContent.includes('SECOND:Beta')"), true);
  unchanged(program, await snapshot(), "two stdin entries");
  assert.deepEqual(await savedBytes(program.buffer.path), programBytes, "program input changed source bytes");

  // A Send click returns to the input while the next PULL waits. Leaving the
  // console after Send must survive that later asynchronous state change.
  await runCurrentFile();
  await waitFor(() => evaluate("document.querySelector('.run-state')?.textContent === 'Waiting for input'"), "Send test first PULL", 30000);
  await waitFor(() => active("rexx-input"), "Send test console focus");
  await command("Input.insertText", {text: "Gamma"});
  await click('.console-input button[type="submit"]');
  assert.equal(await active("rexx-input"), true, "Send button did not restore console focus");
  await click(".console-output");
  await waitFor(() => evaluate("document.querySelector('.console-output')?.textContent.includes('FIRST:Gamma') && document.querySelector('.run-state')?.textContent === 'Waiting for input'"), "Send test second PULL", 30000);
  await waitFor(() => evaluate("document.activeElement?.classList.contains('console-output')"), "explicit console departure");
  assert.equal(await active("rexx-input"), false, "asynchronous console state stole focus after departure");
  await evaluate("document.querySelector('#rexx-input').focus()");
  await enterInput("Delta");
  await waitFor(() => evaluate("document.querySelector('.run-state')?.textContent === 'Completed · status 0'"), "Send test completion", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output')?.textContent.includes('SECOND:Delta')"), true);
  unchanged(program, await snapshot(), "Send and explicit departure");
  assert.deepEqual(await savedBytes(program.buffer.path), programBytes);
  console.log("PASS browser input focus: successive THE commands, Execute, Escape/F6, two Classic PULLs, Send, explicit leave and source preservation");
}
