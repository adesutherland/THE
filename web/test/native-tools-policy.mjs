import assert from "node:assert/strict";
import WebSocket from "ws";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";

// The same check can be called by the Wasm browser harness with
// expectedReason="unsupported-host". The native restricted host uses policy.
export async function checkNativeToolPolicy(request, expectedReason, inspect) {
  const command = text => request({type: "command", command: text});
  const before = await command("query workbench capabilities");
  const revision = (await inspect("SUMMARY")).revision;
  const expectRefusal = async text => {
    const snapshot = await command(text);
    const summary = await inspect("SUMMARY");
    assert.equal(summary.lastResult.code, expectedReason, text);
    assert.equal(summary.revision, revision, "refusal publishes no definitions");
    assert.deepEqual(snapshot.buffer, before.buffer);
    assert.deepEqual(snapshot.focus, before.focus);
  };
  await command("workbench begin owner public.browsernative");
  await expectRefusal('workbench tool put public.browserpython role runner adapter native-process executable "/explicit/native/rexx" source snapshot');
  await command("workbench abort");
  await command("workbench begin owner public.browsernative");
  await expectRefusal('workbench action put public.browserrun label "Run" tool public.browserpython source active ARG "untrusted"');
  await command("workbench abort");
  await expectRefusal('workbench run public.browserpython source active ARG "untrusted"');
}

if (process.env.THE_WEB_URL && process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const url = new URL(process.env.THE_WEB_URL);
  url.protocol = "ws:";
  url.pathname = "/ws";
  const socket = new WebSocket(url), records = [];
  socket.on("message", data => records.push(JSON.parse(data.toString())));
  const wait = async (predicate, from = 0) => {
    const deadline = Date.now() + 30000;
    while (Date.now() < deadline) {
      const result = records.slice(from).find(predicate);
      if (result) return result;
      await new Promise(resolve => setTimeout(resolve, 20));
    }
    throw new Error("Native tool policy response timeout: " + JSON.stringify(records.slice(-3)));
  };
  let id = 100;
  const request = async message => {
    const from = records.length, current = id++;
    socket.send(JSON.stringify({v: 1, id: current, ...message}));
    const ack = await wait(r => r.type === "ack" && r.id === current, from);
    return wait(r => r.screen_rows, records.indexOf(ack) + 1);
  };
  const inspect = async section => {
    const from = records.length;
    socket.send(JSON.stringify({v: 1, type: "workbench", version: 1, operation: "list", section, offset: 0, limit: 8}));
    return (await wait(r => r.type === "workbench" && r.operation === "list" && r.data?.section === section.toLowerCase(), from)).data;
  };
  try {
    await wait(r => r.screen_rows);
    await checkNativeToolPolicy(request, "policy-denied", inspect);
    console.log("Restricted native browser refuses tool declaration/action/run with no editor mutation");
  } finally {
    socket.close();
  }
}
