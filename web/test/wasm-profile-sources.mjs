import assert from "node:assert/strict";

// Visible Files/Tools controls open real buffers. IDBFS checks prove that
// Save and startup seeding preserve the exact edited source bytes on reload.
export async function profileSourcesSmoke({ send, evaluate, command, waitFor, snapshot, hasCrexx }) {
  const names = hasCrexx
    ? ["browser.the", "quickfind.the", "quickopen.the", "quickline.the", "paneldemo.the"]
    : ["browser.commands"];
  async function durable(path) {
    return evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
      open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
      const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
      get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
      get.onerror=()=>{reject(get.error);db.close();};};})`);
  }
  async function openFile(name, directory = "") {
    await evaluate(`(() => {const button=[...document.querySelectorAll('.pane-switcher button')]
      .find(node=>node.textContent==='Files');if(!document.querySelector('.file-explorer')?.getClientRects().length)button.click();})()`);
    if (!await evaluate("!!document.querySelector('.root-switcher button')?.getClientRects().length")) {
      await waitFor(() => evaluate("(() => {const button=document.querySelector('.file-explorer .pane-expand');return button?.getClientRects().length && button.textContent==='Expand pane';})()"), "visible Files Expand control");
      await evaluate("document.querySelector('.file-explorer .pane-expand').click()");
    }
    await waitFor(() => evaluate("!!document.querySelector('.root-switcher button')?.getClientRects().length"), "visible Files roots");
    await evaluate("document.querySelector('.root-switcher button').click()");
    await waitFor(() => evaluate("document.querySelector('.explorer-path > span')?.textContent==='/'"), "workspace Files root");
    if (directory) {
      await waitFor(() => evaluate(`[...document.querySelectorAll('.file-list button')]
        .some(node=>node.textContent.trim()===${JSON.stringify(directory)})`), `${directory} directory`);
      await evaluate(`[...document.querySelectorAll('.file-list button')]
        .find(node=>node.textContent.trim()===${JSON.stringify(directory)}).click()`);
    }
    await waitFor(() => evaluate(`[...document.querySelectorAll('.file-list button')]
      .some(node=>node.textContent.trim()===${JSON.stringify(name)}&&node.getClientRects().length)`), `${name} visible in Files`);
    await evaluate(`[...document.querySelectorAll('.file-list button')]
      .find(node=>node.textContent.trim()===${JSON.stringify(name)}).click()`);
    const path = `/workspace/${directory ? directory + "/" : ""}${name}`;
    await waitFor(async () => (await snapshot()).buffer?.path === path, `${name} editor buffer`);
    await waitFor(() => evaluate("!!document.querySelector('.editor-scroll')?.getClientRects().length && !!document.querySelector('.key-hints')?.getClientRects().length"), `${name} returns to visible editor and profile footer`);
    assert.equal((await snapshot()).buffer.dirty, 0);
    return path;
  }
  async function prependAndSave(marker) {
    await send({ type: "command", command: "cursor goto 1 1" });
    await waitFor(async () => {
      const state = await snapshot();
      return state.focus.zone === "filearea" && state.focus.line === 1 && state.focus.cell === 0;
    }, "profile source cursor");
    await evaluate("document.querySelector('.input-sink').focus()");
    await command("Input.insertText", { text: marker });
    await waitFor(async () => (await snapshot()).buffer.dirty &&
      (await snapshot()).screen_rows.some(row => row.line === 1 && row.t.startsWith(marker)), "profile source DOM edit");
    await evaluate("document.querySelector('[aria-label=Save]').click()");
    await waitFor(async () => !(await snapshot()).buffer.dirty, "profile source durable Save");
  }

  // Compact source navigation returns to the editor after choosing a file or
  // Edit profile. The remaining persistence lifecycle has a known wide layout.
  await command("Emulation.setDeviceMetricsOverride", {width: 820, height: 500, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  const sourceBeforeFilesClose = await snapshot();
  for (const method of ["close", "toggle"]) {
    await evaluate("[...document.querySelectorAll('.pane-switcher button')].find(node=>node.textContent==='Files').click()");
    await waitFor(() => evaluate("!!document.querySelector('.file-explorer .pane-expand')?.getClientRects().length"), "compact Files heading");
    await evaluate("document.querySelector('.file-explorer .pane-expand').click()");
    await waitFor(() => evaluate("!!document.querySelector('.root-switcher button')?.getClientRects().length"), "expanded Files body before close");
    await evaluate(method === "close" ? "document.querySelector('[aria-label=\"Close files\"]').click()"
      : "[...document.querySelectorAll('.pane-switcher button')].find(node=>node.textContent==='Files').click()");
    await waitFor(() => evaluate("!document.querySelector('.file-explorer') && !document.querySelector('.workspace').classList.contains('aux-expanded') && !!document.querySelector('.editor-scroll')?.getClientRects().length && !!document.querySelector('.key-hints')?.getClientRects().length"), `expanded Files ${method} returns to visible editor/footer`);
    const after = await snapshot();
    assert.deepEqual({ buffer: after.buffer, source: after.source }, { buffer: sourceBeforeFilesClose.buffer, source: sourceBeforeFilesClose.source }, "closing Files preserves source bytes, dirty state and identity");
  }
  await openFile(names[0], "Profile sources");
  assert.equal(await evaluate("document.querySelector('.workspace').classList.contains('aux-expanded')"), false, "choosing a file leaves expanded Files mode");
  const jobBeforeSourceNavigation = await evaluate("JSON.stringify({job:window.__tools.job,chunks:window.__tools.chunks})");
  await evaluate("document.querySelector('.tools-toggle').click()");
  await waitFor(() => evaluate("(() => {const button=document.querySelector('.auxiliary-view-controls button');return button?.getClientRects().length && button.textContent==='Expand pane';})()"), "compact Tools Expand control");
  await evaluate("document.querySelector('.auxiliary-view-controls button').click()");
  await waitFor(() => evaluate("!!document.querySelector('.tools-profiles-pane')?.getClientRects().length"), "expanded compact Tools and profiles");
  await evaluate("[...document.querySelectorAll('.tools-sections button')].find(node=>node.textContent==='Profiles').click()");
  await evaluate("document.querySelector('.compiled-profile-setup summary').click()");
  await evaluate("(() => {const input=document.querySelector('[aria-label=\"Compiled profile path\"]');input.value='/workspace/retained.profile.rxbin';input.dispatchEvent(new Event('input',{bubbles:true}));})()");
  await evaluate("[...document.querySelectorAll('.profile-setup button')].find(node=>node.textContent==='Edit profile').click()");
  await waitFor(async () => (await snapshot()).buffer?.path === "/workspace/.crxthe-tools/user.profile.the", "compact Edit profile opens the actual source");
  await waitFor(() => evaluate("!!document.querySelector('.editor-scroll')?.getClientRects().length && !!document.querySelector('.key-hints')?.getClientRects().length"), "Edit profile returns to visible editor and footer");
  assert.equal(await evaluate("document.querySelector('.workspace').classList.contains('aux-expanded')"), false);
  assert.equal(await evaluate("document.querySelector('[aria-label=\"Compiled profile path\"]').value"), "/workspace/retained.profile.rxbin", "source navigation retains the setup draft");
  assert.equal(await evaluate("JSON.stringify({job:window.__tools.job,chunks:window.__tools.chunks})"), jobBeforeSourceNavigation, "source navigation preserves program job and output");
  await command("Emulation.setDeviceMetricsOverride", {width: 1424, height: 836, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  await evaluate("[...document.querySelectorAll('.pane-switcher button')].find(node=>node.textContent==='Files').click()");
  await waitFor(() => evaluate("!!document.querySelector('.file-explorer')?.getClientRects().length"), "wide Files initially visible");
  // A wide layout can already render retained Tools beside Files. Its button
  // now toggles the visible target, so only activate it when Tools is hidden.
  await evaluate("(() => {const button=document.querySelector('.tools-toggle');if(button.getAttribute('aria-pressed')!=='true')button.click();})()");
  await waitFor(() => evaluate("!!document.querySelector('.tools-profiles-pane')?.getClientRects().length"), "wide Tools body");
  await evaluate("document.querySelector('.tools-heading .pane-expand').click()");
  await waitFor(() => evaluate("document.querySelector('.workspace').classList.contains('aux-expanded')"), "wide expanded Tools");
  assert.equal(await evaluate("[...document.querySelectorAll('.pane-switcher button')].find(node=>node.textContent==='Files').getAttribute('aria-pressed')"), "true", "Files can remain logically open while expanded Tools hides its picker");
  await openFile(names[0], "Profile sources");
  assert.equal(await evaluate("document.querySelector('.workspace').classList.contains('aux-expanded')"), false, "Files opened from wide expanded Tools reveals the picker and returns to editor after selection");
  assert.equal(await evaluate("document.querySelector('[aria-label=\"Compiled profile path\"]').value"), "/workspace/retained.profile.rxbin", "wide Files navigation preserves setup draft");
  assert.equal(await evaluate("JSON.stringify({job:window.__tools.job,chunks:window.__tools.chunks})"), jobBeforeSourceNavigation, "wide Files navigation preserves program state");
  console.log("PASS source navigation: compact Files Expand/select/close/toggle and Tools Edit profile, wide expanded Tools to visible Files/editor, retained setup draft/program state");
  await command("Emulation.setDeviceMetricsOverride", {width: 1200, height: 900, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");

  const originals = new Map();
  for (const name of names) {
    const path = await openFile(name, "Profile sources");
    const bytes = await durable(path);
    assert.ok(bytes?.length, `${name} seeded durably before startup`);
    originals.set(path, bytes);
    const firstLine = Buffer.from(bytes).toString("utf8").split("\n")[0];
    assert.ok((await snapshot()).screen_rows.some(row => row.line === 1 && row.t.length && firstLine.startsWith(row.t)),
      `${name} source is visible in the editor`);
  }
  const stockPath = await openFile(names[0], "Profile sources");
  const marker = "/* Saved source copy 界 */ ";
  await prependAndSave(marker);
  const stockBytes = Buffer.from(await durable(stockPath)).toString("utf8");
  assert.equal(stockBytes, marker + Buffer.from(originals.get(stockPath)).toString("utf8"));

  // Open the separate user override from Tools & Profiles, while the Program
  // console remains dedicated to program input and output.
  await evaluate(`(() => {const button=document.querySelector('.tools-toggle');
    if(button.getAttribute('aria-pressed')!=='true')button.click();})()`);
  await waitFor(() => evaluate("!!document.querySelector('.tools-profiles-pane')?.getClientRects().length"), "visible Tools and profiles pane");
  await evaluate("[...document.querySelectorAll('.tools-sections button')].find(node=>node.textContent==='Profiles').click()");
  assert.equal(await evaluate("!!document.querySelector('.tools-profiles-pane .profile-setup button')?.getClientRects().length && !document.querySelector('.rexx-dock .profile-setup')"), true);
  await evaluate("[...document.querySelectorAll('.tools-profiles-pane .profile-setup button')].find(node=>node.textContent==='Edit profile').click()");
  const userPath = "/workspace/.crxthe-tools/user.profile.the";
  await waitFor(async () => (await snapshot()).buffer?.path === userPath, "separate user profile");
  await prependAndSave("/* Saved user override 界 */ ");
  const userBytes = await durable(userPath);
  assert.ok(Buffer.from(userBytes).toString("utf8").startsWith("/* Saved user override 界 */ "));

  const generation = await evaluate("window.__profileSourcesGeneration = Math.random()");
  await command("Page.reload", { ignoreCache: true });
  await waitFor(() => evaluate(`window.__profileSourcesGeneration!==${generation}&&!!window.__snapshot`), "profile source reload", 30000);
  await openFile(names[0], "Profile sources");
  assert.equal(Buffer.from(await durable(stockPath)).toString("utf8"), stockBytes,
    "startup never overwrites an edited stock source copy");
  assert.ok((await snapshot()).screen_rows.some(row => row.line === 1 && row.t.startsWith(marker)));
  await openFile("user.profile.the", ".crxthe-tools");
  assert.deepEqual(await durable(userPath), userBytes, "editable user override survives reload");
  assert.ok((await snapshot()).screen_rows.some(row => row.line === 1 && row.t.startsWith("/* Saved user override 界 */ ")));
  for (const [path, bytes] of originals) {
    if (path !== stockPath) assert.deepEqual(await durable(path), bytes, "other source snapshots remain exact");
  }
  await openFile("Welcome.txt");
  console.log("PASS browser profile sources: visible Files/editor access, editable stock copies, separate user override, exact durable Save and reload without overwrite");
}

// Called after a successful build, before the ordinary activation/restore smoke.
// An applied profile cannot undo arbitrary editor commands, so the receipt must
// distinguish an in-session application from a durably saved selection.
export async function profileActivationStorageFailureSmoke({ send, evaluate, command, waitFor, snapshot }) {
  const targets = await command("Target.getTargets");
  const worker = targets.targetInfos.find(target => target.type === "worker" && target.url.includes("wasm-worker"));
  assert.ok(worker, "editor Worker available for storage refusal injection");
  const { sessionId } = await command("Target.attachToTarget", { targetId: worker.targetId, flatten: true });
  const durableSelection = () => evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
    const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/workspace/.crxthe-tools/active');
    get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
    get.onerror=()=>{reject(get.error);db.close();};};})`);
  const previous = await durableSelection();
  await command("Runtime.evaluate", { expression: `globalThis.profileOriginalPut=IDBObjectStore.prototype.put;
    IDBObjectStore.prototype.put=function(){throw new DOMException('profile quota refusal','QuotaExceededError');};` }, sessionId);
  try {
    await send({ type: "tool", action: "activate" });
    await waitFor(async () => (await snapshot()).last_message ===
      "Profile applied; browser storage save failed. Retry Activate", "activation storage refusal receipt", 30000);
    assert.deepEqual(await durableSelection(), previous, "failed activation sync preserves durable selection");
  } finally {
    await command("Runtime.evaluate", { expression: "IDBObjectStore.prototype.put=globalThis.profileOriginalPut" }, sessionId);
    await command("Target.detachFromTarget", { sessionId });
  }
  await send({ type: "tool", action: "activate" });
  await waitFor(async () => (await snapshot()).last_message === "Profile activated", "activation persistence retry", 30000);
  assert.ok(await durableSelection(), "successful retry persists the selected artifact");
  console.log("PASS browser profile activation: quota refusal reports applied but unsaved selection; retry persists it");
}
