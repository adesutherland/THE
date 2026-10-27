/// <reference lib="webworker" />

type Request = Record<string, unknown>;
type FS = {
  mkdir(path: string): void;
  mount(type: unknown, options: object, path: string): void;
  syncfs(populate: boolean, callback: (error?: Error) => void): void;
  analyzePath(path: string): { exists: boolean };
  writeFile(path: string, text: string): void;
  chdir(path: string): void;
  readdir(path: string): string[];
  lstat(path: string): { mode: number };
  isDir(mode: number): boolean;
  isFile(mode: number): boolean;
};
type Editor = {
  FS: FS;
  IDBFS: unknown;
  ENV: Record<string, string>;
  ccall(name: string, result: string, types: string[], values: unknown[], options: object): Promise<number>;
};

const send = (message: object) => self.postMessage(message);
const pending: Request[] = [];
let wake: ((request: Request | null) => void) | undefined;
let editor: Editor | undefined;

function sync(populate = false) {
  return new Promise<void>((resolve, reject) => editor!.FS.syncfs(populate, (error) => error ? reject(error) : resolve()));
}

async function flushWorkspace() {
  send({ type: "storage", state: "saving" });
  try {
    await sync();
    send({ type: "storage", state: "saved" });
  } catch (error) {
    send({ type: "storage", state: "error", message: "Browser storage save failed. Your buffer is still unsaved." });
    throw error;
  }
}

function nextEditorRequest(timeout: number): Promise<Request | null> {
  if (pending.length) return Promise.resolve(pending.shift()!);
  return new Promise((resolve) => {
    let timer: ReturnType<typeof setTimeout> | undefined;
    wake = (request) => { wake = undefined; if (timer) clearTimeout(timer); resolve(request); };
    if (timeout >= 0) timer = setTimeout(() => wake?.(null), timeout);
  });
}

function files(request: Request) {
  try {
    const relative = request.path ?? "";
    if (request.root !== undefined && request.root !== 0) throw new Error("Invalid workspace root");
    if (typeof relative !== "string" || relative.startsWith("/") || relative.split(/[\\/]/).includes("..") || relative.includes("\0")) throw new Error("Invalid workspace path");
    const directory = "/workspace" + (relative ? "/" + relative : "");
    const fs = editor!.FS;
    const entries = fs.readdir(directory).filter((name) => name !== "." && name !== "..").flatMap((name) => {
      const target = directory + "/" + name;
      const mode = fs.lstat(target).mode;
      if (!fs.isDir(mode) && !fs.isFile(mode)) return [];
      return [{ name, path: relative ? relative + "/" + name : name, target, type: fs.isDir(mode) ? "directory" : "file", readonly: false }];
    });
    send({ type: "files", id: request.id, root: 0, path: relative, roots: [{ id: 0, name: "Browser workspace", readonly: false }], entries });
  } catch (error) {
    send({ type: "error", id: request.id, message: String(error) });
  }
}

self.onmessage = ({ data }: MessageEvent<Request>) => {
  // C resumes only through nextEditorRequest. Never reenter suspended Wasm.
  if (!editor) return;
  if (!data || typeof data !== "object" || Array.isArray(data) || typeof data.type !== "string") {
    send({ type: "error", message: "Invalid browser request" }); return;
  }
  if (data.type === "files") { files(data); return; }
  if (pending.length >= 256) { send({ type: "error", message: "Editor input queue is full" }); return; }
  if (wake) wake(data);
  else pending.push(data);
};

async function start() {
  const url = new URL("../wasm/crxthe_wasm.mjs", import.meta.url);
  const { default: createEditor } = await import(/* @vite-ignore */ url.href);
  editor = await createEditor({
    noInitialRun: true,
    locateFile: (name: string) => new URL(name, url).href,
    sendEditorMessage: send, nextEditorRequest, flushWorkspace,
    print: (text: string) => console.debug(text),
    printErr: (text: string) => console.warn(text),
  }) as Editor;
  const fs = editor.FS;
  fs.mkdir("/workspace");
  fs.mount(editor.IDBFS, {}, "/workspace");
  await sync(true);
  if (!fs.analyzePath("/workspace/Welcome.txt").exists) {
    fs.writeFile("/workspace/Welcome.txt", "Welcome to CRXTHE in your browser.\n\nCtrl-S saves to this browser workspace.\nFiles remain here after you reload.\n");
    await sync();
  }
  fs.chdir("/workspace");
  const hasCrexx = await editor.ccall("the_wasm_has_crexx", "number", [], [], {});
  for (const [name, value] of Object.entries({
    HOME: "/home/web_user", THE_HOME_DIR: "/app/", THE_WEB_WORKSPACE: "/workspace",
    THE_WEB_READONLY_ROOTS: "/app",
    THE_WEB_PROFILE: hasCrexx ? "/app/browser.rxbin" : "/app/browser.commands",
    THE_REXX_INTERPRETER: hasCrexx ? "CREXX" : "NONE", THE_WEB_MACRO_PATH: "/app:/workspace",
  })) editor.ccall("setenv", "number", ["string", "string", "number"], [name, value, 1], {});
  send({ type: "storage", state: "saved" });
  send({ type: "ready" });
  const result = await editor.ccall("the_wasm_main", "number", [], [], { async: true });
  send({ type: "ended", result });
}

async function acquireWorkspace() {
  if (!navigator.locks) throw new Error("This browser does not provide workspace locking");
  await navigator.locks.request("crxthe-browser-workspace", { ifAvailable: true }, async (lock) => {
    if (!lock) throw new Error("This workspace is already open in another tab. Close that tab and reload.");
    await start();
  });
}

acquireWorkspace().catch((error) => send({ type: "fatal", message: String(error) }));
