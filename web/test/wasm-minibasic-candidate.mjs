import assert from "node:assert/strict";
import { createHash } from "node:crypto";

/** Optional candidate acceptance through the same Files/Console/editor UI. */
export async function minibasicCandidateSmoke({
  pageUrl, manifest, send, snapshot, evaluate, command, waitFor, openWorkspaceFile, consoleEnter,
}) {
  if (!manifest.capabilities.minibasic_candidate) return;
  for (const capability of ["editor_address", "profile_source_compiler", "classic_rexx_highlighter", "classic_rexx_interpreter"])
    assert.equal(manifest.capabilities[capability], true, `MiniBASIC candidate requires ${capability}`);

  async function asset(name, binary = false) {
    const relative = `examples/${name}`;
    const response = await fetch(new URL(relative, pageUrl));
    assert.ok(response.ok, `MiniBASIC candidate asset missing: ${relative} (HTTP ${response.status})`);
    const bytes = Buffer.from(await response.arrayBuffer());
    const receipt = manifest.files[relative];
    assert.ok(receipt, `MiniBASIC candidate asset absent from manifest: ${relative}`);
    assert.equal(bytes.length, receipt.bytes, `${relative} manifest size`);
    assert.equal(createHash("sha256").update(bytes).digest("hex"), receipt.sha256, `${relative} manifest hash`);
    if (binary) assert.equal(bytes.subarray(0, 8).toString(), "cReXx007", `${relative} RXBIN header`);
    return bytes;
  }
  const runner = await asset("MiniBASIC.rxbin", true);
  const language = await asset("MiniBASICHighlight.rxbin", true);
  const original = await asset("MiniBASIC.bas");
  const sourceLines = original.toString("utf8").split(/\r\n|\r|\n/);
  const sumLine = sourceLines.findIndex(line => line === '100 PRINT "SUM =", TOTAL') + 1;
  const letLine = sourceLines.findIndex(line => line === "20 LET N = 10") + 1;
  assert.ok(sumLine && letLine && sourceLines.includes("120 INPUT VALUE"),
    "MiniBASIC.bas must be the maintained basic1.bas interactive SUM=55 sample");

  const durable = path => evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
    const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
    get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
    get.onerror=()=>{reject(get.error);db.close();};};})`);
  const sourcePath = "/workspace/MiniBASIC.bas";
  assert.deepEqual(await durable(sourcePath), [...original], "candidate source seeded with exact packaged bytes");

  async function showConsole() {
    await evaluate(`(() => {const button=document.querySelector('.console-toggle');
      if(button.getAttribute('aria-pressed')!=='true')button.click();})()`);
    await waitFor(() => evaluate("!!document.querySelector('.rexx-dock')?.getClientRects().length"), "visible MiniBASIC console");
  }
  async function profileButton(label) {
    await showConsole();
    await evaluate("(() => {const options=document.querySelector('.profile-options');if(!options.open)options.querySelector('summary').click();})()");
    await waitFor(() => evaluate(`[...document.querySelectorAll('.profile-actions button')]
      .some(button=>button.textContent===${JSON.stringify(label)}&&!button.disabled&&button.getClientRects().length)`), `visible ${label}`);
    await evaluate(`[...document.querySelectorAll('.profile-actions button')].find(button=>button.textContent===${JSON.stringify(label)}).click()`);
  }
  async function importImage(name, bytes) {
    assert.equal(await durable(`/workspace/${name}`), null, `${name} left for the visible Import RXBIN flow`);
    await showConsole();
    await evaluate(`(() => {const options=document.querySelector('.console-options');
      if(!options.open)options.querySelector('summary').click();})()`);
    await waitFor(() => evaluate("[...document.querySelectorAll('.rexx-dock button')].some(button=>button.textContent==='Import RXBIN'&&button.getClientRects().length&&!button.disabled)"), "visible Import RXBIN control");
    await evaluate(`(() => {
      const bytes=Uint8Array.from(atob(${JSON.stringify(bytes.toString("base64"))}),c=>c.charCodeAt(0));
      const transfer=new DataTransfer();
      transfer.items.add(new File([bytes],${JSON.stringify(name)},{type:'application/octet-stream'}));
      const input=document.querySelector('input[type=file][accept=".rxbin"]');
      input.files=transfer.files;input.dispatchEvent(new Event('change',{bubbles:true}));
    })()`);
    await waitFor(() => evaluate(`window.__messages.some(message=>message.type==='tool-imported'&&message.path===${JSON.stringify(`/workspace/${name}`)})`), `${name} browser import`, 30000);
    assert.deepEqual(await durable(`/workspace/${name}`), [...bytes], `${name} durable import bytes`);
  }
  await importImage("MiniBASIC.rxbin", runner);
  await importImage("MiniBASICHighlight.rxbin", language);

  // Registrations come from editable source, then the real browser compiler
  // and ADDRESS THE activation; no registry or filesystem injection is used.
  await profileButton("Edit profile");
  const profilePath = "/workspace/.crxthe-tools/profile.the";
  await waitFor(async () => (await snapshot()).buffer.path === profilePath, "MiniBASIC editable profile");
  const profileLines = [
    "/* MiniBASIC candidate profile */", "options levelb", "import rxfnsb", "address THE",
    "'rx register minibasic runner /workspace/MiniBASIC.rxbin .bas \"MiniBASIC\"';",
    "'rx register minibasic-syntax language /workspace/MiniBASICHighlight.rxbin .bas \"MiniBASIC syntax\"';",
    "say 'MiniBASIC candidate profile activated'", "return 0",
  ];
  await send({ type: "command", command: "top" });
  await send({ type: "command", command: "delete *" });
  await send({ type: "command", command: "top" });
  for (const line of profileLines) await send({ type: "command", command: `input ${line}` });
  await send({ type: "command", command: "cursor goto 1 1" });
  await waitFor(async () => (await snapshot()).focus.line === 1 && (await snapshot()).focus.cell === 0, "profile source edit position");
  await evaluate("document.querySelector('.input-sink').focus()");
  const marker = "/* Edited in browser 界 */ ";
  await command("Input.insertText", { text: marker });
  await waitFor(async () => (await snapshot()).buffer.dirty &&
    (await snapshot()).screen_rows.some(row => row.line === 1 && row.t.startsWith(marker)), "visible MiniBASIC profile source edit");
  await evaluate("document.querySelector('[aria-label=Save]').click()");
  await waitFor(async () => !(await snapshot()).buffer.dirty, "MiniBASIC profile source Save");
  const savedProfile = await durable(profilePath);
  assert.equal(Buffer.from(savedProfile).toString("utf8").trimEnd(), marker + profileLines.join("\n"), "inspectable registration source bytes");
  const previousBuild = await evaluate("window.__tools.job.id");
  await profileButton("Build profile");
  await waitFor(() => evaluate(`window.__tools.job.id>${previousBuild}&&window.__tools.job.tool==='profile-compiler'&&!window.__tools.job.running`), "MiniBASIC browser profile build", 60000);
  assert.equal(await evaluate("window.__tools.job.state"), "Completed · status 0", "MiniBASIC source profile compiled");
  await profileButton("Activate profile");
  await waitFor(() => evaluate("window.__tools.chunks.some(chunk=>chunk.text.includes('MiniBASIC candidate profile activated'))"), "MiniBASIC addressed profile activation", 30000);
  await waitFor(async () => (await snapshot()).last_message === "Profile activated", "durable MiniBASIC profile activation");
  const selectedProfile = await durable("/workspace/.crxthe-tools/active");
  assert.ok(selectedProfile?.length, "immutable MiniBASIC profile selection persisted");
  const registry = await durable("/workspace/.crxthe-tools/tools.registry");
  assert.ok(registry?.length, "MiniBASIC registrations persisted");
  for (const [id, role, image] of [["minibasic", "runner", "MiniBASIC.rxbin"], ["minibasic-syntax", "language", "MiniBASICHighlight.rxbin"]])
    assert.equal(await evaluate(`window.__tools.catalog.some(tool=>tool.id===${JSON.stringify(id)}&&tool.role===${JSON.stringify(role)}&&tool.image===${JSON.stringify(`/workspace/${image}`)}&&tool.extensions==='.bas'&&tool.available)`), true, `${id} registered by profile`);

  await openWorkspaceFile("MiniBASIC.bas");
  const ready = () => waitFor(async () => {
    const state = await snapshot();
    return state.parser?.state === "ready" && state.parser.providerId === "minibasic-syntax" && state.parser.applied === state.parser.revision;
  }, "current MiniBASIC parser revision", 30000);
  await ready();
  assert.equal((await snapshot()).diagnostics?.length ?? 0, 0, "valid MiniBASIC source has no diagnostics");
  await send({ type: "command", command: `cursor goto ${letLine} 1` });
  await waitFor(async () => (await snapshot()).screen_rows.some(row => row.line === letLine &&
    row.s?.some(([start, length, style]) => start === 0 && length === 2 && style === "number") &&
    row.s.some(([start, length, style]) => start === 3 && length === 3 && style === "keyword")), "MiniBASIC number and keyword spans");
  assert.equal(await evaluate("!!document.querySelector('.syntax-keyword')&&!!document.querySelector('.syntax-number')"), true, "MiniBASIC syntax visible in editor");
  await send({ type: "command", command: "set insertmode on" });
  await evaluate("document.querySelector('.input-sink').focus()");
  await command("Input.insertText", { text: "[" });
  await waitFor(async () => {
    const state = await snapshot();
    return state.parser?.state === "ready" && state.parser.applied === state.parser.revision &&
      state.diagnostics?.some(diagnostic => diagnostic.code === "L_INPUT" && diagnostic.line === letLine && diagnostic.column === 1);
  }, "located MiniBASIC syntax diagnostic", 30000);
  await send({ type: "command", command: `cursor goto ${letLine} 1` });
  await waitFor(async () => (await snapshot()).status_pmsg?.severity === "ERROR", "MiniBASIC cursor-local diagnostic");
  assert.equal(await evaluate("!!document.querySelector('.syntax-diagnostic-error')&&!!document.querySelector('.status-pmsg-error')"), true, "MiniBASIC error visible at cursor");
  await send({ type: "action", action: "edit.undo" });
  await ready();
  await waitFor(async () => !(await snapshot()).buffer.dirty && !(await snapshot()).diagnostics?.length, "MiniBASIC syntax repair");

  // Keep the edit in the current working record. Run must capture it without
  // saving it or substituting the durable BASIC file for the source snapshot.
  const sumText = sourceLines[sumLine - 1];
  await send({ type: "command", command: `cursor goto ${sumLine} ${sumText.indexOf("SUM =") + 1}` });
  await waitFor(async () => (await snapshot()).focus.line === sumLine &&
    (await snapshot()).focus.cell === sumText.indexOf("SUM ="), "unsaved MiniBASIC print position");
  await evaluate("document.querySelector('.input-sink').focus()");
  await command("Input.insertText", { text: "UNSAVED " });
  await waitFor(async () => (await snapshot()).buffer.dirty && (await snapshot()).screen_rows.some(row => row.t.includes('PRINT "UNSAVED SUM ="')), "MiniBASIC unsaved working record");
  const beforeRun = await snapshot();
  await showConsole();
  await evaluate("(() => {const field=document.querySelector('[aria-label=\"Rexx arguments\"]');field.value='';field.dispatchEvent(new Event('input',{bubbles:true}));})()");
  const runFromEditor = async () => evaluate("document.querySelector('.pane-switcher .run-rexx').click()");
  await runFromEditor();
  await waitFor(() => evaluate("window.__tools.job.tool==='minibasic'&&window.__tools.job.waiting"), "MiniBASIC INPUT waiting", 30000);
  const active = await evaluate("window.__tools.job");
  assert.equal(active.filename, sourcePath);
  assert.equal(active.sourceId, beforeRun.source.id);
  assert.equal(active.revision, beforeRun.source.revision);
  assert.ok(await evaluate("document.querySelector('.console-output').textContent.includes('UNSAVED SUM = 55')"), "actual MiniBASIC loop executed unsaved source");
  await consoleEnter("5");
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), "MiniBASIC interactive completion", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output').textContent.includes('The number is non-negative: 5')"), true, "MiniBASIC INPUT reached positive branch");
  assert.ok((await snapshot()).buffer.dirty, "MiniBASIC Run preserves unsaved state");
  assert.deepEqual(await durable(sourcePath), [...original], "MiniBASIC Run preserves saved source bytes");
  await send({ type: "action", action: "edit.undo" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "restore unsaved MiniBASIC edit before reload");

  const generation = await evaluate("window.__minibasicGeneration=Math.random()");
  await command("Page.reload", { ignoreCache: true });
  await waitFor(() => evaluate(`window.__minibasicGeneration!==${generation}&&window.__tools?.chunks.some(chunk=>chunk.text.includes('MiniBASIC candidate profile activated'))`), "MiniBASIC profile restored after reload", 30000);
  assert.deepEqual(await durable(profilePath), savedProfile, "editable MiniBASIC profile source survives reload");
  assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), selectedProfile, "compiled profile selection survives reload");
  assert.deepEqual(await durable("/workspace/.crxthe-tools/tools.registry"), registry, "MiniBASIC registrations survive reload");
  assert.deepEqual(await durable(sourcePath), [...original], "saved MiniBASIC source survives reload");
  await openWorkspaceFile("MiniBASIC.bas");
  await ready();
  await runFromEditor();
  await waitFor(() => evaluate("window.__tools.job.tool==='minibasic'&&window.__tools.job.waiting"), "restored MiniBASIC runner INPUT", 30000);
  await consoleEnter("-5");
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), "restored MiniBASIC runner completion", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output').textContent.includes('SUM = 55')&&!document.querySelector('.console-output').textContent.includes('UNSAVED SUM')&&document.querySelector('.console-output').textContent.includes('The number is negative: -5')"), true, "restored BASIC bytes and negative INPUT branch");
  console.log("PASS MiniBASIC candidate: manifest/import, visible editable profile build/activate, syntax/located diagnostic/repair, unsaved INPUT run and durable reload");
}
