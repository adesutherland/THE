/// <reference lib="webworker" />

type Request = Record<string, unknown>;
type FS = {
  mkdir(path: string): void;
  mount(type: unknown, options: object, path: string): void;
  syncfs(populate: boolean, callback: (error?: Error) => void): void;
  analyzePath(path: string): { exists: boolean };
  writeFile(path: string, text: string | Uint8Array): void;
  readFile(path: string): Uint8Array;
  chdir(path: string): void;
  readdir(path: string): string[];
  lstat(path: string): { mode: number };
  isDir(mode: number): boolean;
  isFile(mode: number): boolean;
  unlink(path: string): void;
};
type Editor = {
  FS: FS;
  IDBFS: unknown;
  ENV: Record<string, string>;
  ccall(
    name: string,
    result: string,
    types: string[],
    values: unknown[],
    options: object,
  ): Promise<number>;
};

const send = (message: object) => self.postMessage(message);
const pending: Request[] = [];
let wake: ((request: Request | null) => void) | undefined;
let editor: Editor | undefined;
type HighlightJob = {
  id: number;
  revision: number;
  source: Uint8Array;
  lines: number;
  image: string;
};
let highlighter:
  | { submit(job: HighlightJob): void; forget(id: number): void; reset(): void }
  | undefined;

function enqueue(request: Request) {
  if (wake) wake(request);
  else pending.push(request);
}

function sync(populate = false) {
  return new Promise<void>((resolve, reject) =>
    editor!.FS.syncfs(populate, (error) => (error ? reject(error) : resolve())),
  );
}

async function flushWorkspace() {
  send({ type: "storage", state: "saving" });
  try {
    await sync();
    send({ type: "storage", state: "saved" });
  } catch (error) {
    send({
      type: "storage",
      state: "error",
      message: "Browser storage save failed. Your buffer is still unsaved.",
    });
    throw error;
  }
}

function nextEditorRequest(timeout: number): Promise<Request | null> {
  if (pending.length) return Promise.resolve(pending.shift()!);
  return new Promise((resolve) => {
    let timer: ReturnType<typeof setTimeout> | undefined;
    wake = (request) => {
      wake = undefined;
      if (timer) clearTimeout(timer);
      resolve(request);
    };
    if (timeout >= 0) timer = setTimeout(() => wake?.(null), timeout);
  });
}

function files(request: Request) {
  try {
    const relative = request.path ?? "";
    if (request.root !== undefined && request.root !== 0)
      throw new Error("Invalid workspace root");
    if (
      typeof relative !== "string" ||
      relative.startsWith("/") ||
      relative.split(/[\\/]/).includes("..") ||
      relative.includes("\0")
    )
      throw new Error("Invalid workspace path");
    const directory = "/workspace" + (relative ? "/" + relative : "");
    const fs = editor!.FS;
    const entries = fs
      .readdir(directory)
      .filter((name) => name !== "." && name !== "..")
      .flatMap((name) => {
        const target = directory + "/" + name;
        const mode = fs.lstat(target).mode;
        if (!fs.isDir(mode) && !fs.isFile(mode)) return [];
        return [
          {
            name,
            path: relative ? relative + "/" + name : name,
            target,
            type: fs.isDir(mode) ? "directory" : "file",
            readonly: false,
          },
        ];
      });
    send({
      type: "files",
      id: request.id,
      root: 0,
      path: relative,
      roots: [{ id: 0, name: "Browser workspace", readonly: false }],
      entries,
    });
  } catch (error) {
    send({ type: "error", id: request.id, message: String(error) });
  }
}

self.onmessage = ({ data }: MessageEvent<Request>) => {
  // C resumes only through nextEditorRequest. Never reenter suspended Wasm.
  if (!editor) return;
  if (
    !data ||
    typeof data !== "object" ||
    Array.isArray(data) ||
    typeof data.type !== "string"
  ) {
    send({ type: "error", message: "Invalid browser request" });
    return;
  }
  if (data.type === "files") {
    files(data);
    return;
  }
  if (pending.length >= 256) {
    send({ type: "error", message: "Editor input queue is full" });
    return;
  }
  if (data.type === "parser-restart") highlighter?.reset();
  enqueue(data);
};

type ToolJob = {
  id: number;
  sourceId: number;
  revision: number;
  role: string;
  entry: string;
  image: string;
  filename: string;
  source: Uint8Array;
  arguments: string[];
};
let activeTool:
  | {
      id: number;
      abort: AbortController;
      process?: any;
      worker?: Worker;
      timer?: ReturnType<typeof setTimeout>;
    }
  | undefined;
function toolEvent(id: number, event: string, fields: object = {}) {
  enqueue({ type: "tool-event", job: id, event, ...fields });
}
function stopTool(id: number) {
  if (activeTool?.id !== id) return;
  activeTool.abort.abort();
  activeTool.worker?.terminate();
  clearTimeout(activeTool.timer);
  activeTool = undefined;
}
async function startTool(job: ToolJob) {
  const own = {
    id: job.id,
    abort: new AbortController(),
    process: undefined as any,
    worker: undefined as Worker | undefined,
    timer: undefined as ReturnType<typeof setTimeout> | undefined,
    remaining: 120000,
    deadline: 0,
    waiting: false,
    started: false,
  };
  activeTool = own;
  const current = () => activeTool === own;
  const event = (kind: string, fields: object = {}) => {
    if (current()) toolEvent(job.id, kind, fields);
  };
  const finish = (fields: object) => {
    event("finish", fields);
    stopTool(job.id);
  };
  const arm = () => {
    clearTimeout(own.timer);
    own.deadline = Date.now() + own.remaining;
    own.timer = setTimeout(
      () => finish({ code: -1, error: "Execution time limit reached" }),
      own.remaining,
    );
  };
  const inputState = (waiting: boolean) => {
    if (!current()) return;
    if (own.started && !own.waiting)
      own.remaining = Math.max(1, own.deadline - Date.now());
    own.waiting = waiting;
    clearTimeout(own.timer);
    if (own.started && !waiting) arm();
    event("waiting", { waiting: waiting ? 1 : 0 });
  };
  try {
    if (job.role === "compiler") {
      const worker = new Worker(
        new URL("../vm/profile-compiler.mjs", import.meta.url),
        { type: "module" },
      );
      own.worker = worker;
      worker.onmessage = async ({ data }) => {
        if (!current()) return;
        if (data.type === "output")
          event("output", { fd: data.fd, bytes: data.bytes });
        else if (data.type === "failed")
          finish({ code: -1, error: data.error });
        else if (data.type === "built") {
          try {
            const path = `/workspace/.crxthe-tools/build-${crypto.randomUUID()}.rxbin`;
            editor!.FS.writeFile(path, data.bytes);
            await sync();
            if (!current()) return;
            event("built", {
              path,
              sourceId: job.sourceId,
              revision: job.revision,
            });
            finish({ code: 0, status: "0" });
          } catch (error) {
            finish({ code: -1, error: String(error) });
          }
        }
      };
      worker.onerror = (e) => finish({ code: -1, error: e.message });
      worker.postMessage(job);
      event("started");
      own.started = true;
      arm();
    } else {
      const url = new URL(
        /* @vite-ignore */ "../vm/run-host.mjs",
        import.meta.url,
      );
      const host = await import(/* @vite-ignore */ url.href);
      const options = {
        source: job.source,
        filename: job.filename,
        arguments: job.arguments,
        signal: own.abort.signal,
        stdout: (bytes: Uint8Array) => event("output", { fd: 1, bytes }),
        stderr: (bytes: Uint8Array) => event("output", { fd: 2, bytes }),
        inputState,
      };
      if (job.role === "runner") own.process = await host.startRexx(options);
      else {
        const vm = await import(
          /* @vite-ignore */ new URL("../vm/vm-client.mjs", import.meta.url)
            .href
        );
        own.process = await vm.CrexxVMProcess.start({
          ...options,
          images: [
            editor!.FS.readFile("/app/library.rxbin"),
            editor!.FS.readFile(job.image),
          ],
          arguments: job.arguments,
          workerUrl: new URL("../vm/runner/vm-worker.mjs", import.meta.url),
        });
      }
      if (!current()) {
        own.abort.abort();
        return;
      }
      event("started");
      own.started = true;
      if (!own.waiting) arm();
      const result = await own.process.exit;
      if (current())
        finish({
          code: result.runtimeCode,
          status: String(result.programCode ?? 0),
          error: result.error ?? "",
          wire: result.result ?? "",
        });
    }
  } catch (error) {
    if (current()) finish({ code: -1, error: String(error) });
  }
}
function toolInput(id: number, bytes: Uint8Array) {
  if (activeTool?.id !== id || !activeTool.process) return false;
  void activeTool.process
    .write(bytes)
    .catch((error: unknown) =>
      toolEvent(id, "finish", { code: -1, error: String(error) }),
    );
  return true;
}
function toolEof(id: number) {
  if (activeTool?.id !== id || !activeTool.process) return false;
  void activeTool.process
    .endInput()
    .catch((error: unknown) =>
      toolEvent(id, "finish", { code: -1, error: String(error) }),
    );
  return true;
}
async function importTool(request: Request) {
  let path = "";
  try {
    const name = request.name,
      base64 = request.bytes;
    if (
      typeof name !== "string" ||
      !name.endsWith(".rxbin") ||
      /[\\/\0]/.test(name) ||
      name.length > 240 ||
      typeof base64 !== "string" ||
      base64.length > 89478488
    )
      throw Error("Invalid RXBIN import");
    const raw = atob(base64);
    if (raw.length > 67108864 || raw.slice(0, 8) !== "cReXx007")
      throw Error("Invalid RXBIN image");
    path = "/workspace/" + name;
    if (editor!.FS.analyzePath(path).exists) {
      path = "";
      throw Error("A workspace file with that name already exists");
    }
    const bytes = Uint8Array.from(raw, (c) => c.charCodeAt(0));
    editor!.FS.writeFile(path, bytes);
    await sync();
    send({ type: "tool-imported", path });
  } catch (error) {
    if (path && editor!.FS.analyzePath(path).exists) editor!.FS.unlink(path);
    send({ type: "error", message: String(error) });
  }
}
async function start() {
  const url = new URL("../wasm/crxthe_wasm.mjs", import.meta.url);
  const { default: createEditor } = await import(/* @vite-ignore */ url.href);
  editor = (await createEditor({
    noInitialRun: true,
    locateFile: (name: string) => new URL(name, url).href,
    sendEditorMessage: send,
    nextEditorRequest,
    flushWorkspace,
    startTool,
    stopTool,
    toolInput,
    toolEof,
    importTool,
    requestHighlight: (job: HighlightJob) => highlighter?.submit(job),
    forgetHighlight: (id: number) => highlighter?.forget(id),
    print: (text: string) => console.debug(text),
    printErr: (text: string) => console.warn(text),
  })) as Editor;
  if (await editor.ccall("the_wasm_has_highlighter", "number", [], [], {})) {
    const moduleUrl = new URL(
      /* @vite-ignore */ "../vm/classic-host.mjs",
      import.meta.url,
    );
    const { ClassicHighlighter } = await import(
      /* @vite-ignore */ moduleUrl.href
    );
    const services = new Map<string, any>();
    const service = (image: string) => {
      if (!services.has(image))
        services.set(
          image,
          new ClassicHighlighter(
            enqueue,
            image.startsWith("@")
              ? {}
              : { imageLoader: () => editor!.FS.readFile(image) },
          ),
        );
      return services.get(image);
    };
    highlighter = {
      submit: (job) => service(job.image).submit(job),
      forget: (id) => {
        for (const s of services.values()) s.forget(id);
      },
      reset: () => {
        for (const s of services.values()) s.reset();
        services.clear();
      },
    };
  }
  const fs = editor.FS;
  fs.mkdir("/workspace");
  fs.mount(editor.IDBFS, {}, "/workspace");
  await sync(true);
  if (!fs.analyzePath("/workspace/Welcome.txt").exists) {
    fs.writeFile(
      "/workspace/Welcome.txt",
      "Welcome to CRXTHE in your browser.\n\nCtrl-S saves to this browser workspace.\nFiles remain here after you reload.\n",
    );
    await sync();
  }
  if (highlighter && !fs.analyzePath("/workspace/ClassicDemo.rexx").exists) {
    const response = await fetch(
      new URL("../examples/ClassicDemo.rexx", import.meta.url),
    );
    if (!response.ok) throw new Error("Bundled Rexx example unavailable");
    fs.writeFile("/workspace/ClassicDemo.rexx", await response.text());
    await sync();
  }
  if (
    (await editor.ccall("the_wasm_has_interpreter", "number", [], [], {})) &&
    !fs.analyzePath("/workspace/RexxRunner.rexx").exists
  ) {
    const response = await fetch(
      new URL("../examples/RexxRunner.rexx", import.meta.url),
    );
    if (!response.ok)
      throw new Error("Bundled interpreter example unavailable");
    fs.writeFile("/workspace/RexxRunner.rexx", await response.text());
    await sync();
  }
  if (!fs.analyzePath("/workspace/.crxthe-tools").exists)
    fs.mkdir("/workspace/.crxthe-tools");
  fs.chdir("/workspace");
  const hasCrexx = await editor.ccall(
    "the_wasm_has_crexx",
    "number",
    [],
    [],
    {},
  );
  for (const [name, value] of Object.entries({
    THE_TOOL_CONFIG_DIR: "/workspace/.crxthe-tools",
    HOME: "/home/web_user",
    THE_HOME_DIR: "/app/",
    THE_WEB_WORKSPACE: "/workspace",
    THE_WEB_READONLY_ROOTS: "/app",
    THE_WEB_PROFILE: hasCrexx ? "/app/browser.rxbin" : "/app/browser.commands",
    THE_REXX_INTERPRETER: hasCrexx ? "CREXX" : "NONE",
    THE_WEB_MACRO_PATH: "/app:/workspace",
  }))
    editor.ccall(
      "setenv",
      "number",
      ["string", "string", "number"],
      [name, value, 1],
      {},
    );
  send({ type: "storage", state: "saved" });
  send({ type: "ready" });
  const result = await editor.ccall("the_wasm_main", "number", [], [], {
    async: true,
  });
  send({ type: "ended", result });
}

async function acquireWorkspace() {
  if (!navigator.locks)
    throw new Error("This browser does not provide workspace locking");
  await navigator.locks.request(
    "crxthe-browser-workspace",
    { ifAvailable: true },
    async (lock) => {
      if (!lock)
        throw new Error(
          "This workspace is already open in another tab. Close that tab and reload.",
        );
      await start();
    },
  );
}

acquireWorkspace().catch((error) =>
  send({ type: "fatal", message: String(error) }),
);
