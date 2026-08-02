import WebSocket from "ws";

const pageUrl = process.env.THE_WEB_URL;
if (!pageUrl) throw new Error("THE_WEB_URL is required");

const pageResponse = await fetch(pageUrl);
const page = await pageResponse.text();
if (!pageResponse.ok || !page.includes('<div id="app"></div>')) {
  throw new Error(`web UI did not load: HTTP ${pageResponse.status}`);
}

const parsedUrl = new URL(pageUrl);
parsedUrl.protocol = parsedUrl.protocol === "https:" ? "wss:" : "ws:";
parsedUrl.pathname = "/ws";

const socket = new WebSocket(parsedUrl);
const acknowledgements = new Set();
let stage = "initial";
let lastSnapshot = null;
let actionIds = new Set();

const fail = (message) => {
  console.error(message);
  if (lastSnapshot) console.error(JSON.stringify(lastSnapshot));
  process.exit(1);
};

const send = (id, message) => {
  socket.send(JSON.stringify({ v: 1, id, ...message }));
};

const timeout = setTimeout(() => fail(`web runtime timed out at ${stage}`), 10000);

socket.on("message", (data) => {
  let message;
  try {
    message = JSON.parse(data.toString());
  } catch (error) {
    fail(`invalid JSON from web driver: ${error.message}`);
    return;
  }

  if (message.type === "hello") {
    if (message.protocol !== 1 || message.driver !== "web") {
      fail("invalid web driver hello");
    }
    return;
  }
  if (message.type === "actions") {
    actionIds = new Set(message.actions?.map((action) => action.id));
    if (!actionIds.has("file.save") || !actionIds.has("file.close")
        || !actionIds.has("file.open") || !actionIds.has("file.create")) {
      fail("web driver did not expose the required frontend actions");
    }
    return;
  }
  if (message.type === "files") {
    if (stage !== "files" || message.root !== 0 || message.path !== ""
        || !message.entries?.some((entry) => entry.name === "sample.txt"
          && entry.type === "file" && entry.target.endsWith("/sample.txt"))) {
      fail("web workspace listing did not expose the sample file");
    }
    const firstLine = lastSnapshot?.screen_rows.find((row) => row.line === 1);
    if (!firstLine) fail("file listing arrived without an editor snapshot");
    send(1, { type: "hit", target: "filearea", line: 1, row: firstLine.r, cell: 5 });
    stage = "hit";
    return;
  }
  if (message.type === "error") {
    fail(`web driver error: ${message.message}`);
    return;
  }
  if (message.type === "ack") {
    acknowledgements.add(message.id);
    return;
  }
  if (message.mode !== "full" || !Array.isArray(message.screen_rows)) return;

  lastSnapshot = message;
  const firstLine = message.screen_rows.find((row) => row.line === 1);
  if (!firstLine) return;

  if (stage === "initial") {
    if (firstLine.t !== "alpha beta gamma" || message.buffer?.dirty !== 0) {
      fail("initial web snapshot did not match the opened buffer");
    }
    if (message.focus?.zone !== "filearea") {
      fail("web snapshot did not expose the initial editor focus");
    }
    send(10, { type: "files", root: 0, path: "" });
    stage = "files";
    return;
  }

  if (stage === "hit" && acknowledgements.has(1)
      && message.focus?.zone === "filearea" && message.focus.cell === 5) {
    send(2, { type: "command", command: "set insertmode on" });
    stage = "insert-mode";
    return;
  }

  if (stage === "insert-mode" && acknowledgements.has(2)) {
    send(3, { type: "text", text: "X" });
    stage = "text";
    return;
  }

  if (stage === "text" && acknowledgements.has(3)
      && firstLine.t === "alphaX beta gamma") {
    send(4, { type: "key", key: "left" });
    stage = "key";
    return;
  }

  if (stage === "key" && acknowledgements.has(4)
      && message.focus?.cell === 5) {
    send(5, { type: "action", action: "file.save", argument: "" });
    stage = "save";
    return;
  }

  if (stage === "save" && acknowledgements.has(5)
      && firstLine.t === "alphaX beta gamma" && message.buffer?.dirty === 0) {
    send(7, { type: "command", command: "sos execute" });
    stage = "blocked-sos";
    return;
  }

  if (stage === "blocked-sos" && acknowledgements.has(7)
      && message.buffer?.path?.endsWith("/sample.txt")
      && message.status?.includes("unavailable in restricted frontend")) {
    send(8, { type: "command", command: "edit /etc/passwd" });
    stage = "blocked-command";
    return;
  }

  if (stage === "blocked-command" && acknowledgements.has(8)
      && message.buffer?.path?.endsWith("/sample.txt")
      && message.status?.includes("unavailable in restricted frontend")) {
    send(6, { type: "action", action: "file.close", argument: "" });
    stage = "done";
  }
});

socket.on("close", () => {
  clearTimeout(timeout);
  if (stage !== "done") fail(`web driver closed at ${stage}`);
  process.exit(0);
});

socket.on("error", (error) => {
  if (stage === "done") return;
  fail(`websocket error at ${stage}: ${error.message}`);
});
