import WebSocket from "ws";

const url = new URL(process.env.THE_WEB_URL);
url.protocol = "ws:";
url.pathname = "/ws";
const socket = new WebSocket(url);
const records = [];
socket.on("message", data => records.push(JSON.parse(data.toString())));
const wait = async (predicate, from = 0) => {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const result = records.slice(from).find(predicate);
    if (result) return result;
    await new Promise(resolve => setTimeout(resolve, 20));
  }
  throw new Error("workspace policy response timed out: " + JSON.stringify(records.slice(-3)));
};
const send = message => socket.send(JSON.stringify({ v: 1, ...message }));
try {
  await wait(m => m.screen_rows);
  send({ id: 1, type: "files", root: 0, path: "" });
  const listing = await wait(m => m.type === "files" && m.id === 1);
  if (!listing.entries.some(e => e.name === "sample.txt" && e.type === "file"))
    throw new Error("ordinary workspace file was not listed: " + JSON.stringify(listing));
  if (listing.entries.some(e => e.name === "escape")) throw new Error("link was listed");
  send({ id: 2, type: "files", root: 0, path: "escape" });
  await wait(m => m.type === "error" && m.id === 2);
  for (const [id, action, argument] of [[3, "file.open", "escape/secret.txt"],
                                        [4, "file.create", "escape/new.txt"],
                                        [5, "file.open", "absent.txt"]]) {
    const from = records.length;
    send({ id, type: "action", action, argument });
    await wait(m => m.type === "ack" && m.id === id, from);
    const view = await wait(m => m.screen_rows, from);
    if (view.buffers.length !== 1 || !view.buffer.path.replaceAll("\\", "/").endsWith("/sample.txt"))
      throw new Error("disallowed path became an editor buffer: " + JSON.stringify(view.buffer));
  }
  const from = records.length;
  send({ id: 6, type: "action", action: "file.create", argument: "allowed.txt" });
  await wait(m => m.type === "ack" && m.id === 6, from);
  const view = await wait(m => m.screen_rows, from);
  if (!view.buffer.path.replaceAll("\\", "/").endsWith("/allowed.txt"))
    throw new Error("new file inside the workspace was rejected");
  console.log("Workspace policy: link/junction escape and missing-file open rejected; local create passed");
} finally {
  socket.close();
}
