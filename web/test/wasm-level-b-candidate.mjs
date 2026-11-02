import assert from "node:assert/strict";

// Level B is a compiled program here, distinct from the Classic source runner
// and from THE macros/profiles that execute through ADDRESS THE.
export async function levelBCandidateSmoke({ pageUrl, manifest, snapshot, evaluate, waitFor, openWorkspaceFile }) {
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
  await openWorkspaceFile(name);
  await waitFor(() => evaluate(`window.__tools?.currentFile?.path===${JSON.stringify(path)}&&
    window.__tools.currentFile.sourceId===window.__snapshot.source.id&&
    window.__tools.currentFile.revision===window.__snapshot.source.revision`), "Level B current-file actions");
  const before = await snapshot();
  assert.equal(before.buffer.dirty, 0);
  assert.deepEqual(await durable(path), [...sourceBytes], "Level B source seeded with exact bundled bytes");
  assert.equal(await evaluate("window.__tools.currentFile.actions.find(a=>a.id==='build').enabled"), true);
  assert.equal(await evaluate("window.__tools.currentFile.actions.find(a=>a.id==='run').enabled"), false,
    "Level B source requires a build");
  const buildBefore = await evaluate("window.__tools.build.job.id");
  await evaluate("document.querySelector('.file-action-build').click()");
  await waitFor(() => evaluate(`window.__tools.build.job.id>${buildBefore}&&!window.__tools.build.job.running`), "Level B browser source build", 60000);
  assert.equal(await evaluate("window.__tools.build.job.state"), "Completed · status 0");
  const artifact = await evaluate("window.__tools.profile.build");
  assert.equal(artifact.role, "crexx-source");
  assert.equal(artifact.sourceId, before.source.id);
  assert.equal(artifact.revision, before.source.revision);
  assert.equal(artifact.name, "LevelBSquares.rxbin");
  assert.match(artifact.image, /^\/workspace\/\.crxthe-tools\/.*\.rxbin$/);
  assert.equal(Buffer.from(await durable(artifact.image)).subarray(0, 8).toString(), "cReXx007");
  await waitFor(() => evaluate("!document.querySelector('.file-action-run').disabled"), "visible Level B Run enabled");
  assert.equal(await evaluate("window.__tools.currentFile.actions.find(a=>a.id==='run').tool"), "crexx");
  const runBefore = await evaluate("window.__tools.job.id");
  await evaluate("document.querySelector('.file-action-run').click()");
  await waitFor(() => evaluate(`window.__tools.job.id>${runBefore}&&!window.__tools.job.running`), "Level B program completion", 30000);
  const job = await evaluate("window.__tools.job");
  assert.equal(job.tool, "crexx");
  assert.equal(job.state, "Completed · status 0");
  assert.equal(job.sourceId, before.source.id);
  assert.equal(job.revision, before.source.revision);
  assert.equal(await evaluate("window.__tools.chunks.map(c=>c.text).join('')"), "Level B sum of squares: 55\n");
  const after = await snapshot();
  assert.equal(after.buffer.path, path);
  assert.equal(after.buffer.dirty, 0);
  assert.deepEqual(after.focus, before.focus, "Level B Build/Run preserves the logical cursor");
  assert.deepEqual(await durable(path), [...sourceBytes], "Level B Build/Run preserves saved source bytes");
  console.log("PASS browser Level B: bundled .crexx source, selected-file Build/Run, revision-bound isolated VM, sum of squares 55 and exact source preservation");
}
