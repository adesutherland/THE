import assert from "node:assert/strict";

// Exercise the actual optional Wasm compiler and isolated program VM through
// the selected file controls, including unsaved source and durable artifacts.
export async function levelGCandidateSmoke({ pageUrl, manifest, send, snapshot, evaluate, command, waitFor, openWorkspaceFile }) {
  await command("Emulation.setDeviceMetricsOverride", {width: 1200, height: 900, deviceScaleFactor: 1, mobile: false});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  assert.equal(manifest.capabilities.profile_source_compiler, true);
  assert.equal(manifest.capabilities.classic_rexx_interpreter, true);
  const path = "/workspace/LevelGAccumulator.crexx";
  const durable = path => evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
    const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
    get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
    get.onerror=()=>{reject(get.error);db.close();};};})`);
  async function action(id) {
    await waitFor(() => evaluate(`window.__tools?.currentFile?.path===window.__snapshot?.buffer?.path&&
      window.__tools.currentFile.sourceId===window.__snapshot.source.id&&window.__tools.currentFile.revision===window.__snapshot.source.revision`), "current Level G source actions");
    return evaluate(`window.__tools.currentFile.actions.find(action=>action.id===${JSON.stringify(id)})`);
  }
  async function build(success = true) {
    const before = await evaluate("window.__tools.build.job.id");
    assert.equal((await action("build")).enabled, true);
    await evaluate("document.querySelector('.file-action-build').click()");
    await waitFor(() => evaluate(`window.__tools.build.job.id>${before}&&!window.__tools.build.job.running`), "Level G browser build", 60000);
    const state = await evaluate("window.__tools.build.job.state");
    assert.equal(state === "Completed · status 0", success,
      `Level G build state: ${state}; ${await evaluate("window.__tools.build.chunks.map(c=>c.text).join('')")}`);
    return evaluate("window.__tools.profile.build.image");
  }
  async function run(expected, tool = "crexx") {
    const run = await action("run");
    assert.equal(run.enabled, true, run.reason);
    assert.equal(run.tool, tool);
    const before = await evaluate("window.__tools.job.id");
    await evaluate("document.querySelector('.file-action-run').click()");
    await waitFor(() => evaluate(`window.__tools.job.id>${before}&&!window.__tools.job.running`), "Level G program completion", 30000);
    assert.equal(await evaluate("window.__tools.job.state"), "Completed · status 0");
    assert.match(await evaluate("window.__tools.chunks.map(c=>c.text).join('')"), expected);
  }

  await openWorkspaceFile("LevelGAccumulator.crexx");
  const original = Buffer.from(await (await fetch(new URL("examples/LevelGAccumulator.crexx", pageUrl))).arrayBuffer());
  assert.deepEqual(await durable(path), [...original], "seeded Level G source bytes");
  assert.equal((await action("build")).enabled, true, "selected Level G source can build");
  assert.equal((await action("run")).tool, "crexx", "selected Level G source has a compiled program runner");
  assert.equal((await action("run")).enabled, false, "source requires an explicit build");
  assert.match(await evaluate("document.querySelector('.current-file-status').textContent"), /Build this source before running/);
  const firstImage = await build();
  assert.match(firstImage, /^\/workspace\/\.crxthe-tools\/.*\.rxbin$/);
  assert.equal(await evaluate("window.__tools.profile.build.role"), "crexx-source");
  const firstBytes = await durable(firstImage);
  assert.equal(Buffer.from(firstBytes).subarray(0, 8).toString(), "cReXx007");
  await run(/sum of squares: 55\n$/);

  await send({ type: "command", command: "cursor goto 5 1" });
  await send({ type: "command", command: "replace do n = 1 to 6" });
  await waitFor(async () => (await snapshot()).buffer.dirty &&
    (await snapshot()).screen_rows.some(row => row.line === 5 && row.t === "do n = 1 to 6"), "unsaved Level G mutation");
  const edited = await snapshot();
  assert.equal((await action("run")).enabled, false, "editing blocks the previous compiled revision");
  assert.match((await action("run")).reason, /changed.*build/i);
  const beforeRefusal = await evaluate("window.__tools.job.id");
  await send({ type: "tool", action: "run", tool: "crexx", sourceId: edited.source.id, revision: edited.source.revision });
  await waitFor(async () => /changed.*build/i.test((await snapshot()).last_message), "explicit stale build refusal");
  assert.equal(await evaluate("window.__tools.job.id"), beforeRefusal, "stale Run did not launch a VM");
  const secondImage = await build();
  assert.notEqual(secondImage, firstImage, "rebuild gets an immutable new artifact");
  assert.deepEqual(await durable(firstImage), firstBytes, "rebuild preserves the earlier image bytes");
  assert.deepEqual(await durable(path), [...original], "building unsaved source does not save it");
  assert.equal((await snapshot()).buffer.dirty, 1);
  assert.deepEqual((await snapshot()).focus, edited.focus, "build preserves logical cursor");
  await run(/after 6 squares: 91\nsum of squares: 91\n$/);
  assert.deepEqual(await durable(path), [...original], "Run preserves saved source bytes");
  assert.equal((await snapshot()).buffer.dirty, 1);
  assert.deepEqual((await snapshot()).focus, edited.focus, "Run preserves logical cursor");

  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "save edited Level G source");
  const saved = original.toString().replace("do n = 1 to 5", "do n = 1 to 6");
  assert.equal(Buffer.from(await durable(path)).toString(), saved, "exact edited source persisted");
  await openWorkspaceFile("LevelGFibonacci.crexx");
  assert.equal((await action("run")).enabled, false, "another source cannot inherit the previous build");
  await build();
  await run(/10 : 55\n$/);

  const beforeImage = await evaluate("window.__tools.job.id");
  await send({ type: "command", command: 'RX RUN "/workspace/LevelGAccumulator.rxbin"' });
  await waitFor(() => evaluate(`window.__tools.job.id>${beforeImage}&&!window.__tools.job.running`), "explicit precompiled RXBIN completion", 30000);
  assert.equal(await evaluate("window.__tools.job.state"), "Completed · status 0");
  assert.match(await evaluate("window.__tools.chunks.map(c=>c.text).join('')"), /sum of squares: 55\n$/);

  await evaluate(`(() => {const button=document.querySelector('.tools-toggle');
    if(button.getAttribute('aria-pressed')!=='true')button.click();})()`);
  await waitFor(() => evaluate("!!document.querySelector('.tools-profiles-pane')?.getClientRects().length"), "explicit RXBIN tool pane");
  await evaluate("[...document.querySelectorAll('.tools-sections button')].find(node=>node.textContent==='File types & tools').click()");
  await waitFor(() => evaluate("!!document.querySelector('.run-setup')?.getClientRects().length"), "visible File types and tools section");
  await evaluate(`(() => {const select=document.querySelector('[aria-label="Run tool"]');
    select.value='rxbin';select.dispatchEvent(new Event('change',{bubbles:true}));})()`);
  await waitFor(() => evaluate("!!document.querySelector('[aria-label=\"RXBIN path\"]')"), "explicit RXBIN path input");
  await evaluate(`(() => {const input=document.querySelector('[aria-label="RXBIN path"]');
    input.value='/workspace/LevelGFibonacci.rxbin';input.dispatchEvent(new Event('input',{bubbles:true}));})()`);
  const beforeTool = await evaluate("window.__tools.job.id");
  await evaluate("document.querySelector('.run-with-tool').click()");
  await waitFor(() => evaluate(`window.__tools.job.id>${beforeTool}&&!window.__tools.job.running`), "Tools precompiled RXBIN completion", 30000);
  assert.equal(await evaluate("window.__tools.job.state"), "Completed · status 0");
  assert.match(await evaluate("window.__tools.chunks.map(c=>c.text).join('')"), /10 : 55\n$/);

  await openWorkspaceFile("LevelGAccumulator.crexx");
  await send({ type: "command", command: "cursor goto 2 1" });
  await send({ type: "command", command: "replace options invalid-level" });
  await waitFor(async () => (await snapshot()).buffer.dirty, "invalid replacement source");
  await build(false);
  assert.equal((await action("run")).enabled, false, "failed replacement cannot run an earlier image");
  await send({ type: "command", command: "replace options levelg" });
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "save restored Level G source");
  await command("Page.reload", { ignoreCache: true });
  await waitFor(() => evaluate("window.__snapshot?.buffer?.path==='/workspace/Welcome.txt'"), "Level G workspace reload", 30000);
  await openWorkspaceFile("LevelGAccumulator.crexx");
  assert.equal(Buffer.from(await durable(path)).toString(), saved, "reload does not replace the edited source");
  assert.deepEqual(await durable(firstImage), firstBytes, "immutable image survives reload");
  assert.equal((await action("run")).enabled, false, "new session requires a current source build");
  await build();
  await run(/sum of squares: 91\n$/);
  await openWorkspaceFile("Welcome.txt");
  console.log("PASS browser Level G: selected source Build/Run, isolated typed classes/routines, unsaved revision/rebuild/refusal, immutable artifacts, exact Save/reload and explicit RXBIN Run");
}
