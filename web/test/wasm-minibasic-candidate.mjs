import assert from "node:assert/strict";
import { createHash } from "node:crypto";

/** Optional candidate acceptance through the same Files/Console/editor UI. */
export async function minibasicCandidateSmoke({
  pageUrl, manifest, send, snapshot, evaluate, command, waitFor, openWorkspaceFile, consoleEnter, reloadAcceptingUnsaved,
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

  const currentAction = id => evaluate(`window.__tools?.currentFile?.actions.find(action=>action.id===${JSON.stringify(id)})`);
  async function expectCurrentAction(path, id, enabled, tool) {
    await waitFor(() => evaluate(`window.__tools?.currentFile?.path===${JSON.stringify(path)}`), `${path} current-file actions`);
    const action = await currentAction(id);
    assert.equal(action?.enabled, enabled, `${path} ${id} eligibility`);
    assert.equal(action?.target, "current-file", `${path} ${id} is bound to selected file`);
    if (tool) assert.equal(action.tool, tool, `${path} ${id} tool`);
    return action;
  }

  // A fresh BASIC document must not inherit the stock Classic Rexx runner.
  await openWorkspaceFile("MiniBASIC.bas");
  const basicRun = await expectCurrentAction(sourcePath, "run", false);
  assert.ok(basicRun.reason, "unconfigured BASIC explains its Run refusal");
  assert.equal(await evaluate("document.querySelector('.file-action-run')?.disabled"), true,
    "the visible BASIC Run control is disabled before profile setup");
  const preflightJob = await evaluate("window.__tools.job.id");
  await evaluate("document.querySelector('.file-action-run').click()");
  assert.equal(await evaluate("window.__tools.job.id"), preflightJob, "disabled BASIC Run did not start Classic Rexx");
  await openWorkspaceFile("RexxRunner.rexx");
  await expectCurrentAction("/workspace/RexxRunner.rexx", "run", true, "classic-rexx");
  assert.equal(await evaluate("document.querySelector('.file-action-run')?.disabled"), false,
    "the visible Rexx Run control is enabled");
  assert.equal(await evaluate("document.querySelector('.file-action-run + .file-action-context .file-action-tool')?.textContent"), "Classic Rexx",
    "Rexx Run visibly names its selected runner");

  async function showConsole() {
    await evaluate(`(() => {const button=document.querySelector('.console-toggle');
      if(button.getAttribute('aria-pressed')!=='true')button.click();})()`);
    await waitFor(() => evaluate("!!document.querySelector('.rexx-dock')?.getClientRects().length"), "visible MiniBASIC console");
  }
  async function showTools() {
    await evaluate(`(() => {const button=document.querySelector('.tools-toggle');
      if(button.getAttribute('aria-pressed')!=='true')button.click();})()`);
    await waitFor(() => evaluate("!!document.querySelector('.tools-profiles-pane')?.getClientRects().length"), "visible Tools and profiles pane");
    assert.equal(await evaluate("!document.querySelector('.rexx-dock .profile-setup,.rexx-dock [accept=\".rxbin\"]')"), true,
      "setup controls are outside the Program console");
    assert.equal(await evaluate("![...document.querySelectorAll('.tools-profiles-pane [aria-label=\"Run tool\"] option')].some(option=>option.value==='profile-compiler')"), true,
      "the compiler cannot be selected as a program Run tool");
  }
  async function profileButton(label) {
    await showTools();
    await waitFor(() => evaluate(`[...document.querySelectorAll('.tools-profiles-pane .profile-setup button')]
      .some(button=>button.textContent===${JSON.stringify(label)}&&!button.disabled&&button.getClientRects().length)`), `visible ${label}`);
    await evaluate(`[...document.querySelectorAll('.tools-profiles-pane .profile-setup button')].find(button=>button.textContent===${JSON.stringify(label)}).click()`);
  }
  async function importImage(name, bytes) {
    assert.equal(await durable(`/workspace/${name}`), null, `${name} left for the visible Import RXBIN flow`);
    await showTools();
    await waitFor(() => evaluate("[...document.querySelectorAll('.tools-profiles-pane button')].some(button=>button.textContent==='Import RXBIN'&&button.getClientRects().length&&!button.disabled)"), "visible Import RXBIN control");
    await evaluate(`(() => {
      const bytes=Uint8Array.from(atob(${JSON.stringify(bytes.toString("base64"))}),c=>c.charCodeAt(0));
      const transfer=new DataTransfer();
      transfer.items.add(new File([bytes],${JSON.stringify(name)},{type:'application/octet-stream'}));
      const input=document.querySelector('.tools-profiles-pane input[type=file][accept=".rxbin"]');
      input.files=transfer.files;input.dispatchEvent(new Event('change',{bubbles:true}));
    })()`);
    await waitFor(() => evaluate(`window.__messages.some(message=>message.type==='tool-imported'&&message.path===${JSON.stringify(`/workspace/${name}`)})`), `${name} browser import`, 30000);
    assert.deepEqual(await durable(`/workspace/${name}`), [...bytes], `${name} durable import bytes`);
  }
  await importImage("MiniBASIC.rxbin", runner);
  await importImage("MiniBASICHighlight.rxbin", language);

  // Compile a real browser macro from selected .the source. The resulting
  // immutable RXBIN is later declared as this profile's BASIC Build action.
  await send({ type: "action", action: "file.create", argument: "BuildProbe.the" });
  const macroPath = "/workspace/BuildProbe.the";
  await waitFor(async () => (await snapshot()).buffer.path === macroPath, "Build macro source buffer");
  const macroLines = [
    "/* Browser Build probe */", "options levelb", "import rxfnsb",
    "parse arg target", "say 'BUILD-TARGET:'||target", "return 0",
  ];
  await send({ type: "command", command: "top" });
  await send({ type: "command", command: "delete *" });
  await send({ type: "command", command: "top" });
  for (const line of macroLines) await send({ type: "command", command: `input ${line}` });
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "saved Build macro source");
  const savedMacro = await durable(macroPath);
  assert.equal(Buffer.from(savedMacro).toString("utf8").trimEnd(), macroLines.join("\n"),
    "Build macro source bytes are inspectable");
  await expectCurrentAction(macroPath, "build", true, "profile-compiler");
  await expectCurrentAction(macroPath, "activate", false);
  const unbuiltMacroRun = await expectCurrentAction(macroPath, "run", false);
  assert.match(unbuiltMacroRun.reason, /build/i, "unbuilt THE macro explains its Run refusal");
  const macroBuildJob = await evaluate("window.__tools.build?.job.id ?? 0");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build?.job.id>${macroBuildJob}&&window.__tools.build.job.tool==='profile-compiler'&&!window.__tools.build.job.running`), "Build macro RXBIN", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0", "Build macro compiled");
  const macroImage = await evaluate("window.__tools.profile.build.image");
  assert.match(macroImage, /^\/workspace\/\.crxthe-tools\/.*\.rxbin$/, "Build macro has immutable RXBIN image");
  assert.deepEqual(await durable(macroPath), savedMacro, "compiling macro did not save or rewrite its source");
  await expectCurrentAction(macroPath, "run", true, "built-macro");
  const macroRunBefore = await evaluate("window.__tools.job.id");
  await evaluate("document.querySelector('.file-action-run').click()");
  await waitFor(() => evaluate(`window.__tools.job.id>${macroRunBefore}&&window.__tools.job.tool==='built-macro'&&!window.__tools.job.running`), "Run selected built THE macro", 30000);
  assert.equal(await evaluate("window.__tools.fileResult.code"), "completed", "built THE macro completed");
  assert.equal(await evaluate("window.__tools.job.state"), "Completed · status 0", "built THE macro status");
  assert.deepEqual(await durable(macroPath), savedMacro, "running the built macro did not rewrite its source");

  // Registrations come from editable source, then the real browser compiler
  // and ADDRESS THE activation; no registry or filesystem injection is used.
  await profileButton("Edit profile");
  const profilePath = "/workspace/.crxthe-tools/profile.the";
  await waitFor(async () => (await snapshot()).buffer.path === profilePath, "MiniBASIC editable profile");
  const profileLines = [
    "/* MiniBASIC candidate profile */", "options levelb", "import rxfnsb", "address THE",
    "'rx register minibasic runner /workspace/MiniBASIC.rxbin .bas \"MiniBASIC\"';",
    "'rx register minibasic-syntax language /workspace/MiniBASICHighlight.rxbin .bas \"MiniBASIC syntax\"';",
    "'workbench begin owner user.minibasic';",
    `'workbench action put user.basicbuild label "Build BASIC" macro "${macroImage}" fileext ".bas" purpose build';`,
    "'workbench commit';",
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
  const profileBuild = await expectCurrentAction(profilePath, "build", true, "profile-compiler");
  assert.match(profileBuild.label, /Build/, "selected profile has an explicit Build action");
  await expectCurrentAction(profilePath, "activate", false);
  await expectCurrentAction(profilePath, "run", false);
  assert.equal(await evaluate("document.querySelector('.file-action-activate')?.disabled"), true,
    "profile activation waits for a successful build");
  const previousRuntimeJob = await evaluate("window.__tools.job.id");
  const previousBuild = await evaluate("window.__tools.build?.job.id ?? 0");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build?.job.id>${previousBuild}&&window.__tools.build.job.tool==='profile-compiler'&&!window.__tools.build.job.running`), "MiniBASIC browser profile build", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0", "MiniBASIC source profile compiled");
  assert.equal(await evaluate("window.__tools.job.id"), previousRuntimeJob,
    "compiling a profile did not replace Program console history");
  assert.equal(await evaluate("!!document.querySelector('.build-results .build-output')"), true,
    "profile compiler result is inspectable in Build results");
  await expectCurrentAction(profilePath, "activate", true);
  await evaluate("document.querySelector('.file-action-activate').click()");
  await waitFor(() => evaluate("window.__tools.build?.chunks.some(chunk=>chunk.text.includes('MiniBASIC candidate profile activated'))"), "MiniBASIC addressed profile activation", 30000);
  await waitFor(async () => (await snapshot()).last_message === "Profile activated", "durable MiniBASIC profile activation");
  const selectedProfile = await durable("/workspace/.crxthe-tools/active");
  assert.ok(selectedProfile?.length, "immutable MiniBASIC profile selection persisted");
  for (const [id, role, image] of [["minibasic", "runner", "MiniBASIC.rxbin"], ["minibasic-syntax", "language", "MiniBASICHighlight.rxbin"]])
    assert.equal(await evaluate(`window.__tools.catalog.some(tool=>tool.id===${JSON.stringify(id)}&&tool.role===${JSON.stringify(role)}&&tool.image===${JSON.stringify(`/workspace/${image}`)}&&tool.extensions==='.bas'&&tool.available)`), true, `${id} registered by profile`);

  async function reloadSavedProfile(description) {
    const generation = await evaluate("window.__minibasicGeneration=Math.random()");
    await reloadAcceptingUnsaved(generation);
    await waitFor(() => evaluate(`window.__minibasicGeneration!==${generation}&&!!window.__snapshot&&!!window.__tools?.catalog`), description, 30000);
    assert.deepEqual(await durable(profilePath), savedProfile, "editable MiniBASIC profile source survives reload");
    assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), selectedProfile, "compiled profile selection survives reload");
    await waitFor(() => evaluate("window.__tools.catalog.some(tool=>tool.id==='minibasic'&&tool.role==='runner'&&tool.available)&&window.__tools.catalog.some(tool=>tool.id==='minibasic-syntax'&&tool.role==='language'&&tool.available)"),
      "profile-owned MiniBASIC registrations replay on reload", 30000);
    assert.deepEqual(await durable(sourcePath), [...original], "saved MiniBASIC source survives reload");
  }

  await openWorkspaceFile("MiniBASIC.bas");
  const ready = async () => {
    try {
      await waitFor(async () => {
        const state = await snapshot();
        return state.parser?.state === "ready" && state.parser.providerId === "minibasic-syntax" && state.parser.applied === state.parser.revision;
      }, "current MiniBASIC parser revision", 30000);
    } catch (error) {
      const state = await snapshot();
      throw new Error(`${error.message}; parser=${JSON.stringify(state.parser)}; diagnostics=${JSON.stringify(state.diagnostics)}`);
    }
  };
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
  await waitFor(async () => {
    const state = await snapshot();
    return state.focus.line === letLine && state.focus.cell === 0 && state.status_pmsg?.severity === "ERROR";
  }, "MiniBASIC cursor-local diagnostic");
  assert.equal(await evaluate("!!document.querySelector('.syntax-diagnostic-error')&&!!document.querySelector('.status-pmsg-error')"), true, "MiniBASIC error visible at cursor");
  // Moving to the diagnostic commits/reloads the working record, so its line
  // undo baseline now contains '['. Repair that located byte explicitly.
  await evaluate("document.querySelector('.input-sink').focus()");
  await waitFor(() => evaluate("document.activeElement===document.querySelector('.input-sink')"), "MiniBASIC diagnostic repair focus");
  await command("Input.dispatchKeyEvent", { type: "keyDown", key: "Delete", code: "Delete", windowsVirtualKeyCode: 46 });
  await command("Input.dispatchKeyEvent", { type: "keyUp", key: "Delete", code: "Delete", windowsVirtualKeyCode: 46 });
  await waitFor(async () => {
    const state = await snapshot();
    return state.screen_rows.some(row => row.line === letLine && row.t === sourceLines[letLine - 1]) &&
      state.parser?.state === "ready" && state.parser.applied === state.parser.revision && !state.diagnostics?.length;
  }, "MiniBASIC explicit syntax repair", 30000);
  assert.equal(await evaluate("!!document.querySelector('.syntax-diagnostic-error,.status-pmsg-error')"), false, "repaired MiniBASIC clears diagnostic styles");
  assert.deepEqual(await durable(sourcePath), [...original], "syntax edit/repair leaves durable MiniBASIC bytes unchanged");
  // Reload discards the repaired test buffer through the browser lifecycle.
  // Saving would add a final LF that is absent from the maintained sample.
  await reloadSavedProfile("discard repaired MiniBASIC buffer and restore saved profile");
  await openWorkspaceFile("MiniBASIC.bas");
  await ready();
  await waitFor(async () => !(await snapshot()).buffer.dirty && !(await snapshot()).diagnostics?.length, "reopen clean original MiniBASIC source");
  assert.deepEqual(await durable(sourcePath), [...original], "reopened MiniBASIC retains original source and final-LF state");

  const configuredBuild = await expectCurrentAction(sourcePath, "build", true, "user.basicbuild");
  assert.match(configuredBuild.label, /Build BASIC/, "profile declares the selected BASIC Build macro");
  const beforeBuild = await snapshot();
  const runtimeBeforeBuild = await evaluate("window.__tools.job.id");
  const buildReceiptBefore = await evaluate("window.__tools.fileResult?.serial ?? 0");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.fileResult?.serial>${buildReceiptBefore}&&window.__tools.fileResult.action==='build'&&window.__tools.fileResult.code==='completed'`), "profile-defined BASIC Build completion", 30000);
  assert.equal(await evaluate("window.__tools.fileResult.rc"), 0, "profile-defined BASIC Build succeeded");
  assert.deepEqual(await evaluate("({sourceId:window.__tools.fileResult.sourceId,revision:window.__tools.fileResult.revision})"),
    { sourceId: beforeBuild.source.id, revision: beforeBuild.source.revision },
    "Build result is tied to the selected source revision");
  assert.equal(await evaluate("document.querySelector('.build-output')?.textContent.includes('BUILD-TARGET:/workspace/MiniBASIC.bas')"), true,
    "Build macro result is inspectable outside Program console");
  assert.equal(await evaluate("window.__tools.job.id"), runtimeBeforeBuild,
    "Build macro did not replace Program console job history");
  const afterBuild = await snapshot();
  assert.deepEqual(afterBuild.buffer, beforeBuild.buffer, "Build macro preserved selected buffer state");
  assert.deepEqual(afterBuild.source, beforeBuild.source, "Build macro preserved source identity and revision");
  assert.deepEqual(afterBuild.focus, beforeBuild.focus, "Build macro preserved editor focus");
  assert.deepEqual(await durable(sourcePath), [...original], "Build macro did not write saved BASIC bytes");

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
  const dirtyBuild = await expectCurrentAction(sourcePath, "build", false, "user.basicbuild");
  assert.match(dirtyBuild.reason, /save|dirty/i, "unsaved BASIC explains Build refusal");
  await expectCurrentAction(sourcePath, "run", true, "minibasic");
  assert.equal(await evaluate("document.querySelector('.file-action-run + .file-action-context .file-action-tool')?.textContent"), "MiniBASIC",
    "BASIC Run visibly names the profile-selected runner");
  const runFromEditor = async () => evaluate("document.querySelector('.file-action-run').click()");
  await showConsole();
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
  // Reload discards the deliberately unsaved snapshot and restores the saved
  // source/profile without requiring an unrestricted editor quit command.
  await reloadSavedProfile("discard unsaved MiniBASIC run edit and restore saved profile");
  await openWorkspaceFile("MiniBASIC.bas");
  await ready();
  await send({ type: "command", command: `cursor goto ${sumLine} 1` });
  await waitFor(async () => !(await snapshot()).buffer.dirty &&
    (await snapshot()).screen_rows.some(row => row.line === sumLine && row.t === sumText), "reopen clean original MiniBASIC source after reload");
  await runFromEditor();
  await waitFor(() => evaluate("window.__tools.job.tool==='minibasic'&&window.__tools.job.waiting"), "restored MiniBASIC runner INPUT", 30000);
  await consoleEnter("-5");
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), "restored MiniBASIC runner completion", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output').textContent.includes('SUM = 55')&&!document.querySelector('.console-output').textContent.includes('UNSAVED SUM')&&document.querySelector('.console-output').textContent.includes('The number is negative: -5')"), true, "restored BASIC bytes and negative INPUT branch");
  // Replacing the active profile must remove the associations it owned. A
  // reload proves the old BASIC runner is not resurrected from registry state.
  await profileButton("Edit profile");
  await waitFor(async () => (await snapshot()).buffer.path === profilePath, "replacement profile source");
  const replacementLines = ["/* No BASIC associations */", "options levelb", "import rxfnsb",
    "say 'Replacement profile activated'", "return 0"];
  await send({ type: "command", command: "top" });
  await send({ type: "command", command: "delete *" });
  await send({ type: "command", command: "top" });
  for (const line of replacementLines) await send({ type: "command", command: `input ${line}` });
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "replacement profile saved");
  const priorProfileBuild = await evaluate("window.__tools.build.job.id");
  await expectCurrentAction(profilePath, "build", true, "profile-compiler");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build.job.id>${priorProfileBuild}&&!window.__tools.build.job.running`), "replacement profile build", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0", "replacement profile compiled");
  await expectCurrentAction(profilePath, "activate", true);
  await evaluate("document.querySelector('.file-action-activate').click()");
  await waitFor(() => evaluate("window.__tools.build.chunks.some(chunk=>chunk.text.includes('Replacement profile activated'))"), "replacement profile activation", 30000);
  await waitFor(() => evaluate("!window.__tools.catalog.some(tool=>tool.id==='minibasic'||tool.id==='minibasic-syntax')"),
    "removed profile-owned BASIC associations");
  const replacementGeneration = await evaluate("window.__minibasicGeneration=Math.random()");
  await reloadAcceptingUnsaved(replacementGeneration);
  await waitFor(() => evaluate("!!window.__tools?.catalog&&!window.__tools.catalog.some(tool=>tool.id==='minibasic'||tool.id==='minibasic-syntax')"),
    "replacement profile replay on reload", 30000);
  await openWorkspaceFile("MiniBASIC.bas");
  await expectCurrentAction(sourcePath, "run", false);
  await expectCurrentAction(sourcePath, "build", false);
  assert.equal(await evaluate("!window.__tools.catalog.some(tool=>tool.id==='minibasic'||tool.id==='minibasic-syntax')"), true,
    "omitted BASIC association stays removed after reload");
  assert.deepEqual(await durable(sourcePath), [...original], "profile replacement preserved saved BASIC source");
  console.log("PASS MiniBASIC file journey: fresh BASIC refusal, Classic Rexx association, Tools/Profile setup, selected .the Build/Activate, profile BASIC runner and macro Build result, unsaved INPUT run and durable reload");
}
