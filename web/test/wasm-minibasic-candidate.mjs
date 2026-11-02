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
  async function expectRole(path, role) {
    await waitFor(() => evaluate(`window.__tools?.currentFile?.path===${JSON.stringify(path)}`), `${path} role`);
    assert.equal(await evaluate("window.__tools.currentFile.role"), role, `${path} source role`);
  }
  async function expectVisibleActions(ids) {
    await waitFor(() => evaluate(`JSON.stringify([...document.querySelectorAll('.current-file-action-list > button[class*="file-action-"]')]
      .map(node=>[...node.classList].find(name=>name.startsWith('file-action-'))?.slice('file-action-'.length)).sort())===${JSON.stringify(JSON.stringify([...ids].sort()))}`),
      `visible ${ids.join("/")} actions`);
  }

  // A fresh BASIC document runs with its bundled interpreter and highlighter
  // before any user profile is built or applied.
  await openWorkspaceFile("MiniBASIC.bas");
  await waitFor(async () => {
    const state = await snapshot();
    return state.parser?.providerId === "minibasic-syntax" && state.parser.state === "ready"
      && state.parser.applied === state.parser.revision;
  }, "stock MiniBASIC syntax before any user profile", 30000);
  assert.equal(await evaluate("!!document.querySelector('.syntax-keyword')&&!!document.querySelector('.syntax-number')"), true,
    "stock MiniBASIC syntax is visible before user profile setup");
  assert.equal(await durable("/workspace/.crxthe-tools/active"), null, "stock Run requires no saved user profile");
  await expectCurrentAction(sourcePath, "run", true, "minibasic");
  const basicBuild = await expectCurrentAction(sourcePath, "build", false);
  assert.match(basicBuild.reason, /No Build action is associated/, "interpreted BASIC has no stock Build tool");
  await expectVisibleActions(["run"]);
  assert.match(await evaluate("document.querySelector('.file-action-run')?.textContent"), /MiniBASIC/,
    "stock BASIC Run visibly names its interpreter");
  assert.equal(await evaluate("document.querySelector('.file-action-configure')"), null,
    "stock BASIC Run requires no Configure runner step");
  const beforeStockRun = await snapshot();
  const stockRunJob = await evaluate("window.__tools.job.id");
  await showConsole();
  await evaluate("document.querySelector('.file-action-run').click()");
  await waitFor(() => evaluate(`window.__tools.job.id>${stockRunJob}&&window.__tools.job.tool==='minibasic'&&window.__tools.job.waiting`),
    "stock MiniBASIC Run reaches INPUT", 30000);
  const stockJob = await evaluate("window.__tools.job");
  assert.equal(stockJob.filename, sourcePath, "stock Run uses the selected BASIC file");
  assert.equal(stockJob.sourceId, beforeStockRun.source.id, "stock Run captures the selected source identity");
  assert.equal(stockJob.revision, beforeStockRun.source.revision, "stock Run captures the selected source revision");
  assert.equal(await evaluate("document.querySelector('.console-output').textContent.includes('SUM = 55')"), true,
    "stock MiniBASIC Run executes the bundled loop before profile setup");
  await consoleEnter("0");
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), "stock MiniBASIC interactive completion", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output').textContent.includes('The number is non-negative: 0')"), true,
    "stock MiniBASIC INPUT reaches its zero branch");
  const afterStockRun = await snapshot();
  assert.deepEqual(afterStockRun.buffer, beforeStockRun.buffer, "stock Run preserves clean BASIC buffer state");
  assert.deepEqual(afterStockRun.source, beforeStockRun.source, "stock Run preserves BASIC source identity and revision");
  assert.deepEqual(afterStockRun.focus, beforeStockRun.focus, "stock Run preserves BASIC editor focus");
  assert.deepEqual(await durable(sourcePath), [...original], "stock Run preserves exact saved BASIC bytes");
  await openWorkspaceFile("RexxRunner.rexx");
  await expectCurrentAction("/workspace/RexxRunner.rexx", "run", true, "classic-rexx");
  assert.equal(await evaluate("document.querySelector('.file-action-run')?.disabled"), false,
    "the visible Rexx Run control is enabled");
  assert.match(await evaluate("document.querySelector('.file-action-run')?.textContent"), /Classic Rexx/,
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
  assert.deepEqual(await durable("/workspace/MiniBASIC.rxbin"), [...runner],
    "candidate runner seeded with exact packaged bytes");
  assert.deepEqual(await durable("/workspace/MiniBASICHighlight.rxbin"), [...language],
    "candidate language image seeded with exact packaged bytes");

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
  await expectRole(macroPath, "macro-source");
  await expectCurrentAction(macroPath, "build", true, "profile-compiler");
  await expectCurrentAction(macroPath, "activate", false);
  await expectVisibleActions(["build", "run"]);
  assert.equal(await evaluate("document.querySelector('.file-action-activate')"), null,
    "ordinary THE macro has no primary Apply profile action");
  const unbuiltMacroRun = await expectCurrentAction(macroPath, "run", false);
  assert.match(unbuiltMacroRun.reason, /build/i, "unbuilt THE macro explains its Run refusal");
  assert.match(await evaluate("document.querySelector('.current-file-status')?.textContent"), /Build this macro before running/i,
    "macro prerequisite is visible beside its actions");
  assert.notEqual(unbuiltMacroRun.reason, (await currentAction("activate")).reason,
    "macro Run and profile Apply have distinct disabled reasons");
  const macroBuildJob = await evaluate("window.__tools.build?.job.id ?? 0");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build?.job.id>${macroBuildJob}&&window.__tools.build.job.tool==='profile-compiler'&&!window.__tools.build.job.running`), "Build macro RXBIN", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0", "Build macro compiled");
  const macroImage = await evaluate("window.__tools.profile.build.image");
  assert.match(macroImage, /^\/workspace\/\.crxthe-tools\/.*\.rxbin$/, "Build macro has immutable RXBIN image");
  assert.doesNotMatch(macroImage, /\.profile\.rxbin$/, "ordinary macro build is not a profile artifact");
  assert.equal(await evaluate("window.__tools.profile.build.name"), "BuildProbe.rxbin",
    "macro Build exposes its logical output name");
  assert.equal(await evaluate("window.__tools.profile.build.role"), "macro-source");
  assert.equal(await evaluate("window.__tools.profile.build.sourcePath"), macroPath);
  assert.deepEqual(await durable(macroPath), savedMacro, "compiling macro did not save or rewrite its source");
  await expectCurrentAction(macroPath, "run", true, "built-macro");
  assert.match(await evaluate("document.querySelector('.current-file-status')?.textContent"), /Macro built|Run macro/i,
    "macro build outcome is visible beside its actions");
  const macroRunBefore = await evaluate("window.__tools.job.id");
  await evaluate("document.querySelector('.file-action-run').click()");
  await waitFor(() => evaluate(`window.__tools.job.id>${macroRunBefore}&&window.__tools.job.tool==='built-macro'&&!window.__tools.job.running`), "Run selected built THE macro", 30000);
  assert.equal(await evaluate("window.__tools.fileResult.code"), "completed", "built THE macro completed");
  assert.equal(await evaluate("window.__tools.job.state"), "Completed · status 0", "built THE macro status");
  assert.deepEqual(await durable(macroPath), savedMacro, "running the built macro did not rewrite its source");
  await openWorkspaceFile("RexxRunner.rexx");
  await expectRole("/workspace/RexxRunner.rexx", "other");
  await openWorkspaceFile("BuildProbe.the");
  await expectCurrentAction(macroPath, "run", true, "built-macro");
  assert.equal(await evaluate("window.__tools.profile.build.state"), "ready",
    "returning to unchanged macro does not report a stale build");

  // A named profile is compiled from its selected source and applied as a
  // persistent profile. Its image is distinct from an ordinary macro image.
  await send({ type: "action", action: "file.create", argument: "Foo.profile.the" });
  const namedPath = "/workspace/Foo.profile.the";
  await waitFor(async () => (await snapshot()).buffer.path === namedPath, "named profile source buffer");
  const namedLines = [
    "/* Named profile acceptance */", "options levelb", "import rxfnsb", "address THE",
    "'rx register named-profile-probe runner /workspace/MiniBASIC.rxbin .namedprobe \"Named profile probe\"';",
    "say 'Named profile acceptance applied'", "return 0",
  ];
  await send({ type: "command", command: "top" });
  await send({ type: "command", command: "delete *" });
  await send({ type: "command", command: "top" });
  for (const line of namedLines) await send({ type: "command", command: `input ${line}` });
  const namedDirty = await snapshot();
  assert.ok(namedDirty.buffer.dirty, "named profile source is dirty before Save");
  assert.equal(namedDirty.buffer.path, namedPath, "named profile keeps selected source identity");
  assert.notEqual(Buffer.from(await durable(namedPath) ?? []).toString("utf8").trimEnd(), namedLines.join("\n"),
    "unsaved named profile source bytes are not durable");
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "saved named profile source");
  const savedNamed = await durable(namedPath);
  assert.equal(Buffer.from(savedNamed).toString("utf8").trimEnd(), namedLines.join("\n"),
    "named profile Save persists exact source bytes");
  await expectRole(namedPath, "profile-source");
  const namedBuild = await expectCurrentAction(namedPath, "build", true, "profile-compiler");
  assert.match(namedBuild.label, /Build profile/);
  const namedApplyBefore = await expectCurrentAction(namedPath, "activate", false);
  assert.match(namedApplyBefore.reason, /build/i, "unbuilt profile explains Apply refusal");
  assert.match(await evaluate("document.querySelector('.current-file-status')?.textContent"), /Build this profile before applying/i,
    "profile prerequisite is distinct and visible beside its actions");
  await expectVisibleActions(["build", "activate"]);
  assert.equal(await evaluate("document.querySelector('.file-action-run')"), null,
    "profile source has no primary Run macro action");
  const namedBeforeBuild = await snapshot();
  const namedBuildJob = await evaluate("window.__tools.build.job.id");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build.job.id>${namedBuildJob}&&!window.__tools.build.job.running`),
    "named profile build", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0");
  const namedImage = await evaluate("window.__tools.profile.build.image");
  assert.match(namedImage, /^\/workspace\/\.crxthe-tools\/build-[^/]+-[0-9a-f-]{36}\.profile\.rxbin$/i,
    "named profile Build creates an immutable profile RXBIN");
  assert.equal(await evaluate("window.__tools.profile.build.name"), "Foo.profile.rxbin",
    "named profile Build exposes its logical output name");
  assert.notEqual(namedImage, macroImage, "profile and macro images are distinct");
  assert.ok((await durable(namedImage))?.length, "profile RXBIN persists in the browser workspace");
  assert.equal(await evaluate("window.__tools.profile.build.role"), "profile-source");
  assert.equal(await evaluate("window.__tools.profile.build.sourcePath"), namedPath);
  assert.deepEqual(await durable(namedPath), savedNamed, "Build preserves saved named profile bytes");
  assert.equal((await snapshot()).source.id, namedBeforeBuild.source.id, "Build preserves profile source identity");
  assert.match(await evaluate("document.querySelector('.current-file-status')?.textContent"), /Foo\.profile\.rxbin|Apply profile/,
    "profile build outcome explains the next action");
  const priorNamedImage = namedImage;
  const secondNamedJob = await evaluate("window.__tools.build.job.id");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build.job.id>${secondNamedJob}&&!window.__tools.build.job.running`),
    "repeat named profile build", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0");
  const latestNamedImage = await evaluate("window.__tools.profile.build.image");
  assert.notEqual(latestNamedImage, priorNamedImage, "rebuilding creates a new immutable profile path");
  assert.ok((await durable(priorNamedImage))?.length, "earlier profile artifact remains available");
  assert.ok((await durable(latestNamedImage))?.length, "later profile artifact persists");
  await expectCurrentAction(namedPath, "activate", true);
  await openWorkspaceFile("RexxRunner.rexx");
  await openWorkspaceFile("Foo.profile.the");
  await expectCurrentAction(namedPath, "activate", true);
  assert.equal(await evaluate("window.__tools.profile.build.state"), "ready",
    "returning to unchanged named profile does not report a stale build");
  await command("Emulation.setDeviceMetricsOverride", { width: 390, height: 844, deviceScaleFactor: 1, mobile: false });
  await waitFor(() => evaluate("document.querySelector('.file-action-activate')?.getClientRects().length"),
    "phone Apply profile action");
  await evaluate("document.querySelector('.file-action-build').scrollIntoView({block:'center'})");
  assert.equal(await evaluate("(() => { const rect=document.querySelector('.file-action-activate').getBoundingClientRect(); const build=document.querySelector('.file-action-build').getBoundingClientRect(); return rect.left>=0&&rect.right<=innerWidth&&rect.width>0&&build.left>=0&&build.right<=innerWidth&&build.width>0; })()"),
    true, "phone Apply profile control fits viewport");
  await command("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 1 });
  const touchBuildJob = await evaluate("window.__tools.build.job.id");
  const buildCenter = await evaluate("(() => { const rect=document.querySelector('.file-action-build').getBoundingClientRect(); return {x:rect.left+rect.width/2,y:rect.top+rect.height/2}; })()");
  await command("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [buildCenter] });
  await command("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] });
  await waitFor(() => evaluate(`window.__tools.build.job.id>${touchBuildJob}&&!window.__tools.build.job.running`),
    "phone touch Build profile", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0");
  await command("Emulation.setTouchEmulationEnabled", { enabled: false });
  await evaluate("document.querySelector('.file-action-activate').focus()");
  await command("Input.dispatchKeyEvent", { type: "keyDown", key: "Enter", code: "Enter", windowsVirtualKeyCode: 13, text: "\r" });
  await command("Input.dispatchKeyEvent", { type: "keyUp", key: "Enter", code: "Enter", windowsVirtualKeyCode: 13 });
  await waitFor(() => evaluate("window.__tools.build?.chunks.some(chunk=>chunk.text.includes('Named profile acceptance applied'))"),
    "phone keyboard Apply profile", 30000);
  await waitFor(async () => (await snapshot()).last_message === "Profile activated", "named profile activation");
  const selectedNamed = await durable("/workspace/.crxthe-tools/active");
  assert.ok(selectedNamed?.length, "named profile selection is durable");
  await waitFor(() => evaluate("window.__tools.catalog.some(tool=>tool.id==='named-profile-probe'&&tool.extensions==='.namedprobe'&&tool.available)"),
    "named profile registration effect");
  await command("Emulation.clearDeviceMetricsOverride");
  const namedGeneration = await evaluate("window.__minibasicGeneration=Math.random()");
  await reloadAcceptingUnsaved(namedGeneration);
  assert.deepEqual(await durable(namedPath), savedNamed, "named profile source survives reload");
  assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), selectedNamed,
    "named profile selection survives reload");
  await waitFor(() => evaluate("window.__tools.catalog.some(tool=>tool.id==='named-profile-probe'&&tool.extensions==='.namedprobe'&&tool.available)"),
    "named profile registration restores after reload", 30000);

  // Apply a compiled profile brought in through the browser's RXBIN importer.
  // The selected profile must be a private immutable snapshot of those bytes.
  const preparedName = "NamedPrepared.profile.rxbin";
  const preparedPath = `/workspace/${preparedName}`;
  const preparedBytes = Buffer.from(await durable(latestNamedImage));
  await showTools();
  const runToolBeforeProfileImport = await evaluate("document.querySelector('[aria-label=\"Run tool\"]')?.value");
  await importImage(preparedName, preparedBytes);
  await waitFor(() => evaluate(`document.querySelector('[aria-label="Compiled profile path"]')?.value===${JSON.stringify(preparedPath)}`),
    "profile import populates Apply path");
  assert.equal(await evaluate("document.querySelector('[aria-label=\"Run tool\"]')?.value"), runToolBeforeProfileImport,
    "profile import does not select the RXBIN program runner");
  async function applyProfilePath(path) {
    await showTools();
    await waitFor(() => evaluate("!!document.querySelector('[aria-label=\"Compiled profile path\"]')"),
      "compiled profile path control");
    await evaluate(`(() => { const input=document.querySelector('[aria-label="Compiled profile path"]');
      input.value=${JSON.stringify(path)};input.dispatchEvent(new Event('input',{bubbles:true})); })()`);
    await waitFor(() => evaluate(`document.querySelector('[aria-label="Compiled profile path"]').value===${JSON.stringify(path)}`),
      "compiled profile path entry");
    await waitFor(() => evaluate("!document.querySelector('.apply-compiled-profile').disabled"),
      "compiled profile Apply eligibility");
    const serial = await evaluate("window.__tools.fileResult.serial");
    await evaluate("document.querySelector('.apply-compiled-profile').click()");
    return waitFor(() => evaluate(`window.__tools.fileResult.serial>${serial}&&window.__tools.fileResult.action==='apply-profile'&&window.__tools.fileResult`),
      `Apply compiled profile ${path}`, 30000);
  }
  const preparedResult = await applyProfilePath(preparedPath);
  assert.equal(preparedResult.code, "applied", "imported compiled profile applied");
  assert.equal(preparedResult.rc, 0);
  assert.equal(preparedResult.sourceId, 0, "image Apply result is independent of selected editor source");
  const appliedImport = await evaluate("window.__tools.profile.active");
  assert.equal(appliedImport.kind, "compiled-image", "active profile records compiled-image provenance");
  assert.equal(appliedImport.sourcePath, "", "compiled image has no editable source path");
  assert.equal(appliedImport.sourceId, 0, "image Apply has no editor source binding");
  assert.match(appliedImport.image, /^\/workspace\/\.crxthe-tools\/[^/]+\.profile\.rxbin$/,
    "image Apply selects an immutable private profile path");
  assert.notEqual(appliedImport.image, preparedPath, "active selection is separate from imported original");
  assert.ok(Buffer.from(await durable(appliedImport.image)).equals(preparedBytes),
    "active snapshot preserves exact imported bytes");
  assert.match(await evaluate("document.querySelector('.compiled-profile-status')?.textContent"), /applied|active/i,
    "explicit Apply outcome is visible in Tools and profiles");

  const activeSelection = await durable("/workspace/.crxthe-tools/active");
  const missingResult = await applyProfilePath("/workspace/Missing.profile.rxbin");
  assert.notEqual(missingResult.code, "applied", "missing profile image refused");
  assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), activeSelection,
    "invalid path does not replace active profile selection");
  assert.equal(await evaluate("window.__tools.profile.active.image"), appliedImport.image);

  await send({ type: "action", action: "file.create", argument: "Invalid.profile.rxbin" });
  const invalidPath = "/workspace/Invalid.profile.rxbin";
  await waitFor(async () => (await snapshot()).buffer.path === invalidPath, "invalid image fixture buffer");
  await send({ type: "command", command: "top" });
  await send({ type: "command", command: "delete *" });
  await send({ type: "command", command: "top" });
  await send({ type: "command", command: "input Invalid compiled profile image" });
  await waitFor(async () => {
    const state = await snapshot();
    return state.buffer.path === invalidPath && state.buffer.dirty &&
      state.screen_rows.some(row => row.t === "Invalid compiled profile image");
  }, "invalid image fixture source edit");
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => {
    const state = await snapshot(), bytes = await durable(invalidPath);
    return state.buffer.path === invalidPath && !state.buffer.dirty && bytes?.length;
  }, "invalid image fixture saved");
  assert.doesNotMatch(Buffer.from(await durable(invalidPath)).toString("utf8"), /^cReXx007/);
  const invalidSource = await snapshot();
  const invalidResult = await applyProfilePath(invalidPath);
  assert.equal(invalidResult.code, "invalid-image", "bad RXBIN magic refused");
  assert.equal((await snapshot()).source.id, invalidSource.source.id,
    "bad image Apply preserves selected editor source");
  assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), activeSelection,
    "bad magic does not replace active profile selection");

  // The import cannot be overwritten through Import RXBIN, so alter only its
  // persisted original record after Apply and prove the snapshot on restart.
  await evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
      const tx=db.transaction('FILE_DATA','readwrite');const store=tx.objectStore('FILE_DATA');
      const get=store.get(${JSON.stringify(preparedPath)});
      get.onerror=()=>reject(get.error);get.onsuccess=()=>{
        const record=get.result;record.contents=new TextEncoder().encode('invalid original after Apply');
        store.put(record,${JSON.stringify(preparedPath)});};
      tx.oncomplete=()=>{db.close();resolve(true);};tx.onerror=()=>{db.close();reject(tx.error);};};})`);
  assert.notDeepEqual(await durable(preparedPath), [...preparedBytes], "imported original changed after Apply");
  const importedGeneration = await evaluate("window.__minibasicGeneration=Math.random()");
  await reloadAcceptingUnsaved(importedGeneration);
  assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), activeSelection,
    "image Apply selection persists independently of original import");
  assert.ok(Buffer.from(await durable(appliedImport.image)).equals(preparedBytes),
    "immutable active snapshot survives altered original and reload");
  await waitFor(() => evaluate("window.__tools.catalog.some(tool=>tool.id==='named-profile-probe'&&tool.extensions==='.namedprobe'&&tool.available)"),
    "imported profile registration restores after reload", 30000);

  // Keep both supported legacy profile names in the action classification.
  await evaluate("document.querySelector('[aria-label=\"Open\"]').click()");
  await waitFor(() => evaluate("!!document.querySelector('.root-switcher button')"), "Files root switcher");
  await evaluate("document.querySelector('.root-switcher button').click()");
  await waitFor(() => evaluate("document.querySelector('.explorer-path > span')?.textContent==='/'"),
    "workspace Files root");
  await evaluate("[...document.querySelectorAll('.file-list button')].find(node=>node.textContent.trim()==='Profile sources').click()");
  await waitFor(() => evaluate("[...document.querySelectorAll('.file-list button')].some(node=>node.textContent.trim()==='MiniBASIC-profile.the')"),
    "legacy MiniBASIC profile source in Files");
  await evaluate("[...document.querySelectorAll('.file-list button')].find(node=>node.textContent.trim()==='MiniBASIC-profile.the').click()");
  await expectRole("/workspace/Profile sources/MiniBASIC-profile.the", "profile-source");
  await expectVisibleActions(["build", "activate"]);

  // Registrations come from editable source, then the real browser compiler
  // and ADDRESS THE activation; no registry or filesystem injection is used.
  await profileButton("Edit profile");
  const profilePath = "/workspace/.crxthe-tools/user.profile.the";
  await waitFor(async () => (await snapshot()).buffer.path === profilePath, "MiniBASIC editable profile");
  await expectRole(profilePath, "profile-source");
  await expectVisibleActions(["build", "activate"]);
  const profileLines = [
    "/* MiniBASIC candidate profile */", "options levelb", "import rxfnsb", "address THE",
    "'rx register minibasic runner /workspace/MiniBASIC.rxbin .bas \"User MiniBASIC\"';",
    "'rx register minibasic-syntax language /workspace/MiniBASICHighlight.rxbin .bas \"User MiniBASIC syntax\"';",
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
  assert.equal(await evaluate("window.__tools.catalog.find(tool=>tool.id==='minibasic').label"), "User MiniBASIC",
    "user profile overrides the stock BASIC runner definition");
  assert.equal(await evaluate("window.__tools.catalog.find(tool=>tool.id==='minibasic-syntax').label"), "User MiniBASIC syntax",
    "user profile overrides the stock BASIC language definition");

  async function reloadSavedProfile(description) {
    const generation = await evaluate("window.__minibasicGeneration=Math.random()");
    await reloadAcceptingUnsaved(generation);
    await waitFor(() => evaluate(`window.__minibasicGeneration!==${generation}&&!!window.__snapshot&&!!window.__tools?.catalog`), description, 30000);
    assert.deepEqual(await durable(profilePath), savedProfile, "editable MiniBASIC profile source survives reload");
    assert.deepEqual(await durable("/workspace/.crxthe-tools/active"), selectedProfile, "compiled profile selection survives reload");
    await waitFor(() => evaluate("window.__tools.catalog.some(tool=>tool.id==='minibasic'&&tool.role==='runner'&&tool.available)&&window.__tools.catalog.some(tool=>tool.id==='minibasic-syntax'&&tool.role==='language'&&tool.available)"),
      "profile-owned MiniBASIC registrations replay on reload", 30000);
    assert.equal(await evaluate("window.__tools.catalog.find(tool=>tool.id==='minibasic').label"), "User MiniBASIC",
      "saved user runner override wins over stock Run on reload");
    assert.equal(await evaluate("window.__tools.catalog.find(tool=>tool.id==='minibasic-syntax').label"), "User MiniBASIC syntax",
      "saved user language override wins over stock syntax on reload");
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
  assert.match(await evaluate("document.querySelector('.file-action-run')?.textContent"), /MiniBASIC/,
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
  // Replacement removes the user's overrides and custom Build action. Both
  // stock BASIC registrations must remain available after Apply and reload.
  await profileButton("Edit profile");
  await waitFor(async () => (await snapshot()).buffer.path === profilePath, "replacement profile source");
  const replacementLines = ["/* Restore stock BASIC Run and syntax */", "options levelb", "import rxfnsb",
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
  const stockBasic = "window.__tools.catalog.some(tool=>tool.id==='minibasic'&&tool.label==='MiniBASIC'&&tool.role==='runner'&&tool.extensions==='.bas'&&tool.available)&&window.__tools.catalog.some(tool=>tool.id==='minibasic-syntax'&&tool.label==='MiniBASIC syntax'&&tool.role==='language'&&tool.extensions==='.bas'&&tool.available)";
  await waitFor(() => evaluate(stockBasic), "removed user overrides and restored stock BASIC Run and syntax");
  const replacementGeneration = await evaluate("window.__minibasicGeneration=Math.random()");
  await reloadAcceptingUnsaved(replacementGeneration);
  await waitFor(() => evaluate(`!!window.__tools?.catalog&&${stockBasic}`),
    "replacement profile replay on reload", 30000);
  await openWorkspaceFile("MiniBASIC.bas");
  await expectCurrentAction(sourcePath, "run", true, "minibasic");
  const restoredBuild = await expectCurrentAction(sourcePath, "build", false);
  assert.match(restoredBuild.reason, /No Build action is associated/, "replacement removes the custom BASIC Build action");
  await expectVisibleActions(["run"]);
  assert.equal(await evaluate(stockBasic), true, "stock BASIC Run and syntax survive replacement and reload");
  await waitFor(async () => {
    const state = await snapshot();
    return state.parser?.state === "ready" && state.parser.providerId === "minibasic-syntax"
      && state.parser.applied === state.parser.revision;
  }, "stock BASIC parser after profile replacement", 30000);
  assert.equal(await evaluate("!!document.querySelector('.syntax-keyword')&&!!document.querySelector('.syntax-number')"), true,
    "profile replacement preserves visible stock BASIC syntax");
  const replacementRunJob = await evaluate("window.__tools.job.id");
  await showConsole();
  await runFromEditor();
  await waitFor(() => evaluate(`window.__tools.job.id>${replacementRunJob}&&window.__tools.job.tool==='minibasic'&&window.__tools.job.waiting`),
    "stock MiniBASIC Run after profile replacement", 30000);
  await consoleEnter("2");
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), "stock MiniBASIC completion after replacement", 30000);
  assert.equal(await evaluate("document.querySelector('.console-output').textContent.includes('SUM = 55')&&document.querySelector('.console-output').textContent.includes('The number is non-negative: 2')"), true,
    "restored stock BASIC runner executes output and INPUT");
  assert.deepEqual(await durable(sourcePath), [...original], "profile replacement preserved saved BASIC source");
  await send({ type: "action", action: "file.create", argument: "/workspace/.crxthe-tools/profile.the" });
  const legacyPath = "/workspace/.crxthe-tools/profile.the";
  await waitFor(async () => (await snapshot()).buffer.path === legacyPath, "legacy profile.the buffer");
  await expectRole(legacyPath, "profile-source");
  await expectVisibleActions(["build", "activate"]);
  assert.equal(await evaluate("document.querySelector('.file-action-run')"), null,
    "legacy profile.the retains profile actions rather than macro Run");
  console.log("PASS MiniBASIC file journey: stock Run/INPUT, ordinary THE macro Build/Run, named profile immutable Build/Apply/reload, legacy profile names, user runner/syntax overrides and custom Build, unsaved INPUT run, replacement restores stock Run/syntax");
}
