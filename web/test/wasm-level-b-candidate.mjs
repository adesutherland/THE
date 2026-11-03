import assert from "node:assert/strict";

// Level B is a compiled program here, distinct from the Classic source runner
// and from THE macros/profiles that execute through ADDRESS THE.
export async function levelBCandidateSmoke({ pageUrl, manifest, send, snapshot, evaluate, command, waitFor, openWorkspaceFile }) {
  await command("Emulation.setDeviceMetricsOverride", {width: 1200, height: 900, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  assert.equal(manifest.capabilities.source_compiler, true);
  assert.equal(manifest.capabilities.classic_rexx_interpreter, true);
  const name = "LevelBSquares.crexx";
  const path = `/workspace/${name}`;
  const durable = path => evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
    const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
    get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
    get.onerror=()=>{reject(get.error);db.close();};};})`);
  const response = await fetch(new URL(`examples/${name}`, pageUrl));
  assert.equal(response.ok, true, "bundled Level B source is served");
  const sourceBytes = Buffer.from(await response.arrayBuffer());
  assert.match(sourceBytes.toString(), /^options levelb$/m);
  const sourceText = sourceBytes.toString();
  const lines = sourceText.split(/\r\n|\r|\n/);
  const loopLine = lines.indexOf("do n = 1 to 5") + 1;
  const optionsLine = lines.indexOf("options levelb") + 1;
  assert.ok(loopLine && optionsLine, "Level B mutation targets are present");

  async function action(id) {
    await waitFor(() => evaluate(`window.__tools?.currentFile?.path===${JSON.stringify(path)}&&
      window.__tools.currentFile.sourceId===window.__snapshot.source.id&&
      window.__tools.currentFile.revision===window.__snapshot.source.revision`), "Level B current-file actions");
    return evaluate(`window.__tools.currentFile.actions.find(action=>action.id===${JSON.stringify(id)})`);
  }
  async function build(success = true) {
    assert.equal((await action("build")).enabled, true);
    const before = await evaluate("window.__tools.build.job.id");
    await evaluate("document.querySelector('.file-action-build').click()");
    await waitFor(() => evaluate(`window.__tools.build.job.id>${before}&&!window.__tools.build.job.running`), "Level B browser source build", 60000);
    const state = await evaluate("window.__tools.build.job.state");
    assert.equal(state === "Completed · status 0", success,
      `Level B build state: ${state}; ${await evaluate("window.__tools.build.chunks.map(c=>c.text).join('')")}`);
    return evaluate("window.__tools.profile.build");
  }
  async function run(expected) {
    const selected = await action("run");
    assert.equal(selected.enabled, true, selected.reason);
    assert.equal(selected.tool, "crexx");
    await waitFor(() => evaluate("!document.querySelector('.file-action-run').disabled"), "visible Level B Run enabled");
    const before = await evaluate("window.__tools.job.id");
    const source = (await snapshot()).source;
    await evaluate("document.querySelector('.file-action-run').click()");
    await waitFor(() => evaluate(`window.__tools.job.id>${before}&&!window.__tools.job.running`), "Level B program completion", 30000);
    const job = await evaluate("window.__tools.job");
    assert.equal(job.tool, "crexx");
    assert.equal(job.state, "Completed · status 0");
    assert.equal(job.sourceId, source.id);
    assert.equal(job.revision, source.revision);
    assert.equal(await evaluate("window.__tools.chunks.map(c=>c.text).join('')"), expected);
  }
  async function replace(line, text) {
    await send({ type: "command", command: `cursor goto ${line} 1` });
    await send({ type: "command", command: `replace ${text}` });
    await waitFor(async () => {
      const state = await snapshot();
      return state.buffer.dirty && state.screen_rows.some(row => row.line === line && row.t === text);
    }, "Level B source mutation");
  }
  async function save(expected) {
    const focus = (await snapshot()).focus;
    await evaluate("document.querySelector('[aria-label=Save]').click()");
    await waitFor(async () => !(await snapshot()).buffer.dirty, "Level B Save");
    assert.deepEqual((await snapshot()).focus, focus, "Save preserves the logical cursor");
    assert.equal(Buffer.from(await durable(path)).toString(), expected, "exact Level B source saved");
  }
  async function reload(expected) {
    const generation = await evaluate("window.__levelBGeneration=Math.random()");
    await command("Page.reload", { ignoreCache: true });
    await waitFor(() => evaluate(`window.__levelBGeneration!==${generation}&&window.__snapshot?.buffer?.path==='/workspace/Welcome.txt'`), "Level B workspace reload", 30000);
    await openWorkspaceFile(name);
    assert.equal((await snapshot()).buffer.dirty, 0, "reopened Level B source is clean");
    assert.equal(Buffer.from(await durable(path)).toString(), expected, "exact Level B bytes survive reload");
    assert.equal((await action("run")).enabled, false, "new session requires a current source build");
  }

  await openWorkspaceFile(name);
  const before = await snapshot();
  assert.equal(before.buffer.dirty, 0);
  assert.deepEqual(await durable(path), [...sourceBytes], "Level B source seeded with exact bundled bytes");
  assert.equal((await action("run")).enabled, false,
    "Level B source requires a build");
  const artifact = await build();
  assert.equal(artifact.role, "crexx-source");
  assert.equal(artifact.sourceId, before.source.id);
  assert.equal(artifact.revision, before.source.revision);
  assert.equal(artifact.name, "LevelBSquares.rxbin");
  assert.match(artifact.image, /^\/workspace\/\.crxthe-tools\/.*\.rxbin$/);
  const firstImage = await durable(artifact.image);
  assert.equal(Buffer.from(firstImage).subarray(0, 8).toString(), "cReXx007");
  await run("Level B sum of squares: 55\n");
  const after = await snapshot();
  assert.equal(after.buffer.path, path);
  assert.equal(after.buffer.dirty, 0);
  assert.deepEqual(after.focus, before.focus, "Level B Build/Run preserves the logical cursor");
  assert.deepEqual(await durable(path), [...sourceBytes], "Level B Build/Run preserves saved source bytes");

  await replace(loopLine, "do n = 1 to 6");
  const edited = await snapshot();
  const stale = await action("run");
  assert.equal(stale.enabled, false, "editing blocks the previous Level B build");
  assert.match(stale.reason, /changed.*build/i);
  const jobBeforeRefusal = await evaluate("window.__tools.job.id");
  await send({ type: "tool", action: "run", tool: "crexx", sourceId: edited.source.id, revision: edited.source.revision });
  await waitFor(async () => /changed.*build/i.test((await snapshot()).last_message), "stale Level B Run refusal");
  assert.equal(await evaluate("window.__tools.job.id"), jobBeforeRefusal, "stale Level B Run launches no VM");
  const rebuilt = await build();
  assert.equal(rebuilt.sourceId, edited.source.id);
  assert.equal(rebuilt.revision, edited.source.revision, "unsaved Level B build uses the edited revision");
  assert.notEqual(rebuilt.image, artifact.image, "Level B rebuild creates an immutable new artifact");
  assert.deepEqual(await durable(artifact.image), firstImage, "Level B rebuild preserves its previous artifact");
  await run("Level B sum of squares: 91\n");
  assert.equal((await snapshot()).buffer.dirty, 1, "Build/Run preserve dirty Level B source");
  assert.deepEqual((await snapshot()).focus, edited.focus, "unsaved Build/Run preserve the logical cursor");
  assert.deepEqual(await durable(path), [...sourceBytes], "unsaved Build/Run do not save Level B source");

  const saved = sourceText.replace("do n = 1 to 5", "do n = 1 to 6");
  await save(saved);
  await reload(saved);
  await send({ type: "command", command: `cursor goto ${loopLine} 1` });
  await waitFor(async () => (await snapshot()).screen_rows.some(row => row.line === loopLine && row.t === "do n = 1 to 6"), "reopened edited Level B text");
  await build();
  await run("Level B sum of squares: 91\n");

  await replace(optionsLine, "options invalid-level");
  const invalid = await snapshot();
  await build(false);
  assert.equal((await action("run")).enabled, false, "failed Level B build cannot run an earlier artifact");
  assert.equal((await snapshot()).buffer.dirty, 1, "failed Build retains dirty source");
  assert.deepEqual((await snapshot()).focus, invalid.focus, "failed Build preserves the logical cursor");
  assert.equal(Buffer.from(await durable(path)).toString(), saved, "failed Build preserves saved source");
  await replace(optionsLine, "options levelb");
  await replace(loopLine, "do n = 1 to 5");
  await save(sourceText);
  await reload(sourceText);
  await build();
  await run("Level B sum of squares: 55\n");
  await openWorkspaceFile("Welcome.txt");
  console.log("PASS browser Level B: selected Build/Run, unsaved revision and stale refusal, immutable rebuild, exact Save/reload, invalid build refusal and restored original source");
}
