import { useEffect, useRef, useState } from "preact/hooks";
import type { Snapshot } from "./types";

export type RunSource = {
  type: "run-source";
  request: number;
  id: number;
  revision: number;
  source: Uint8Array;
  filename: string;
  error: string;
};
type Process = {
  write(bytes: Uint8Array): Promise<void>;
  endInput(): Promise<void>;
  exit: Promise<{ runtimeCode: number; error?: string; result: string }>;
};
type Chunk = { fd: number; text: string };

export function RexxConsole({
  send,
  source,
  snapshot,
}: {
  send: (message: Record<string, unknown>) => number | undefined;
  source: RunSource | null;
  snapshot: Snapshot | null;
}) {
  const [height, setHeight] = useState(270);
  const [collapsed, setCollapsed] = useState(false);
  const [state, setState] = useState("Ready");
  const [busy, setBusy] = useState(false);
  const [waiting, setWaiting] = useState(false);
  const [ready, setReady] = useState(false);
  const [eof, setEof] = useState(false);
  const [args, setArgs] = useState("");
  const [input, setInput] = useState("");
  const [chunks, setChunks] = useState<Chunk[]>([]);
  const [omitted, setOmitted] = useState(false);
  const [label, setLabel] = useState("");
  const [identity, setIdentity] = useState<{ id: number; revision: number }>();
  const pending = useRef<{ id: number; args: string[] } | null>(null);
  const process = useRef<Process>();
  const abort = useRef<AbortController>();
  const generation = useRef(0);
  const log = useRef<HTMLPreElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  const output = useRef<Chunk[]>([]);
  const outputSize = useRef(0);
  const renderTimer = useRef<number>();
  const timer = useRef<number>();
  const deadline = useRef(0);
  const remaining = useRef(120000);
  const stateRef = useRef(false);

  const append = (fd: number, text: string) => {
    if (!text) return;
    const tail = output.current.at(-1);
    if (tail?.fd === fd) tail.text += text;
    else output.current.push({ fd, text });
    // Bound visible scrollback independently of the total per-run output limit.
    let size = output.current.reduce((n, c) => n + c.text.length, 0);
    while (size > 262144 && output.current.length) {
      const first = output.current[0];
      let trim = Math.min(size - 262144, first.text.length);
      if (
        trim < first.text.length &&
        first.text.charCodeAt(trim) >= 0xdc00 &&
        first.text.charCodeAt(trim) <= 0xdfff
      )
        trim++;
      first.text = first.text.slice(trim);
      size -= trim;
      setOmitted(true);
      if (!first.text) output.current.shift();
    }
    if (renderTimer.current === undefined)
      renderTimer.current = window.setTimeout(() => {
        renderTimer.current = undefined;
        setChunks(output.current.map((c) => ({ ...c })));
      }, 30);
  };
  const flushOutput = () => {
    window.clearTimeout(renderTimer.current);
    renderTimer.current = undefined;
    setChunks(output.current.map(chunk => ({...chunk})));
  };
  const stop = (reason = "Stopped") => {
    generation.current++;
    pending.current = null;
    abort.current?.abort(new Error(reason));
    process.current = undefined;
    window.clearTimeout(timer.current);
    stateRef.current = false;
    flushOutput();
    setBusy(false);
    setReady(false);
    setWaiting(false);
    setState(reason);
  };
  useEffect(
    () => () => {
      abort.current?.abort();
      window.clearTimeout(timer.current);
      window.clearTimeout(renderTimer.current);
    },
    [],
  );
  useEffect(() => {
    const hide = () => stop("Stopped");
    window.addEventListener("pagehide", hide);
    return () => window.removeEventListener("pagehide", hide);
  }, []);
  useEffect(() => {
    if (log.current) log.current.scrollTop = log.current.scrollHeight;
  }, [chunks]);
  useEffect(() => {
    if (waiting && ready) inputRef.current?.focus();
  }, [waiting, ready]);

  useEffect(() => {
    const request = pending.current;
    if (!source || !request || source.request !== request.id) return;
    window.clearTimeout(timer.current);
    pending.current = null;
    if (source.error) {
      setState(source.error);
      setLabel(source.filename);
      setIdentity(undefined);
      setBusy(false);
      return;
    }
    const own = ++generation.current,
      controller = new AbortController();
    abort.current = controller;
    setIdentity({ id: source.id, revision: source.revision });
    setLabel(`${source.filename} · revision ${source.revision}`);
    setState("Starting interpreter…");
    outputSize.current = 0;
    remaining.current = 120000;
    deadline.current = Date.now() + 120000;
    const decoders = [new TextDecoder(), new TextDecoder()];
    const active = () => generation.current === own;
    const armTimer = () => {
      deadline.current = Date.now() + remaining.current;
      timer.current = window.setTimeout(
        () => stop("Execution time limit reached"),
        remaining.current,
      );
    };
    void (async () => {
      try {
        const url = new URL(/* @vite-ignore */ "../vm/run-host.mjs", import.meta.url);
        const { startRexx, parseRunResult } = await import(
          /* @vite-ignore */ url.href
        );
        const stream = (fd: number, bytes: Uint8Array) => {
          if (!active()) return;
          outputSize.current += bytes.length;
          if (outputSize.current > 8388608) {
            stop("Output limit reached (8 MiB)");
            return;
          }
          append(fd, decoders[fd - 1].decode(bytes, { stream: true }));
        };
        const running: Process = await startRexx({
          source: Uint8Array.from(source.source),
          filename: source.filename,
          arguments: request.args,
          signal: controller.signal,
          stdout: (bytes: Uint8Array) => stream(1, bytes),
          stderr: (bytes: Uint8Array) => stream(2, bytes),
          inputState: (isWaiting: boolean) => {
            if (!active()) return;
            window.clearTimeout(timer.current);
            if (!stateRef.current)
              remaining.current = Math.max(1, deadline.current - Date.now());
            stateRef.current = isWaiting;
            if (isWaiting) flushOutput();
            setWaiting(isWaiting);
            setState(isWaiting ? "Waiting for input" : "Running");
            if (!isWaiting) armTimer();
          },
        });
        if (!active()) {
          controller.abort();
          return;
        }
        process.current = running;
        setReady(true);
        if (!stateRef.current) {
          setState("Running");
          armTimer();
        }
        const result = await running.exit;
        if (!active()) return;
        for (let fd = 1; fd <= 2; fd++) append(fd, decoders[fd - 1].decode());
        if (result.runtimeCode || result.error)
          throw new Error(result.error || `VM failure ${result.runtimeCode}`);
        const completion = parseRunResult(result.result);
        if (completion.primary) {
          append(
            2,
            `\n${completion.primary.name}: ${completion.primary.message}\n`,
          );
          setState("Runtime condition");
        } else
          setState(`Completed · status ${completion.status ?? "unavailable"}`);
        if (completion.cleanup)
          append(
            2,
            `\nCleanup: ${completion.cleanup.name}: ${completion.cleanup.message}\n`,
          );
        if (completion.truncated)
          append(2, "\nInterpreter result text was truncated.\n");
      } catch (error) {
        if (active()) {
          setState("Failed");
          append(2, `${String(error)}\n`);
        }
      } finally {
        if (active()) {
          flushOutput();
          window.clearTimeout(timer.current);
          stateRef.current = false;
          setBusy(false);
          setReady(false);
          setWaiting(false);
          process.current = undefined;
        }
      }
    })();
  }, [source]);

  const changed =
    identity &&
    snapshot?.source &&
    (identity.id !== snapshot.source.id ||
      identity.revision !== snapshot.source.revision);
  const run = () => {
    if (busy || pending.current) return;
    setCollapsed(false);
    setBusy(true);
    setReady(false);
    setWaiting(false);
    setEof(false);
    setInput("");
    setState("Capturing buffer…");
    output.current = [];
    setChunks([]);
    setOmitted(false);
    stateRef.current = false;
    const id = send({ type: "run-source" });
    if (id === undefined) {
      setBusy(false);
      setState("Editor unavailable");
      return;
    }
    pending.current = {
      id,
      args: args === "" ? [] : args.replaceAll("\r\n", "\n").split("\n"),
    };
    timer.current = window.setTimeout(
      () => stop("Editor did not return a source snapshot"),
      10000,
    );
  };
  const sendInput = async (event: Event) => {
    event.preventDefault();
    if (!process.current || eof) return;
    const own = generation.current;
    try {
      await process.current.write(new TextEncoder().encode(input + "\n"));
      if (own !== generation.current) return;
      append(0, `› ${input}\n`);
      setInput("");
    } catch (error) {
      if (own === generation.current) stop(String(error));
    }
  };

  return (
    <div
      class={`rexx-dock ${collapsed ? "rexx-collapsed" : ""}`}
      style={{ height: collapsed ? 38 : height }}
    >
      {!collapsed ? (
        <div
          class="console-divider"
          role="separator"
          aria-label="Resize Rexx console"
          aria-orientation="horizontal"
          tabIndex={0}
          aria-valuenow={height}
          aria-valuemin={150}
          aria-valuemax={Math.max(150, window.innerHeight - 250)}
          onKeyDown={(event) => {
            if (event.key === "ArrowUp" || event.key === "ArrowDown") {
              event.preventDefault();
              setHeight((h) =>
                Math.max(
                  150,
                  Math.min(
                    window.innerHeight - 250,
                    h + (event.key === "ArrowUp" ? 20 : -20),
                  ),
                ),
              );
            }
          }}
          onPointerDown={(event) => {
            const target = event.currentTarget;
            target.setPointerCapture(event.pointerId);
            const start = event.clientY,
              previous = height;
            target.onpointermove = (move) =>
              setHeight(
                Math.max(
                  150,
                  Math.min(
                    window.innerHeight - 250,
                    previous + start - move.clientY,
                  ),
                ),
              );
            target.onpointerup = () => {
              target.onpointermove = null;
              target.onpointerup = null;
            };
          }}
        />
      ) : null}
      <div class="console-heading">
        <strong>Rexx console</strong>
        <button
          type="button"
          onClick={run}
          disabled={busy || !snapshot?.buffer || !!snapshot?.panel}
        >
          Run Rexx
        </button>
        <button type="button" onClick={() => stop()} disabled={!busy}>
          Stop
        </button>
        <button
          type="button"
          onClick={() => {
            output.current = [];
            setChunks([]);
            setOmitted(false);
          }}
        >
          Clear
        </button>
        <span class="run-state" role="status">
          {state}
        </span>
        <button
          type="button"
          onClick={() => setCollapsed(!collapsed)}
          aria-label={
            collapsed ? "Expand Rexx console" : "Collapse Rexx console"
          }
        >
          {collapsed ? "Expand" : "Collapse"}
        </button>
      </div>
      {!collapsed ? (
        <>
          <div class="console-context">
            <span title={label}>
              {label || "Run the current buffer, including unsaved edits."}
              {changed ? " · buffer changed" : ""}
              {omitted ? " · older output omitted" : ""}
            </span>
            <label>
              Arguments
              <textarea
                rows={1}
                aria-label="Rexx arguments"
                placeholder="One argument per line"
                value={args}
                disabled={busy}
                onInput={(event) => setArgs(event.currentTarget.value)}
              />
            </label>
          </div>
          <pre
            class="console-output"
            aria-label="Rexx output"
            ref={log}
            tabIndex={0}
          >
            {chunks.map((chunk, index) => (
              <span
                key={index}
                class={
                  chunk.fd === 2
                    ? "console-stderr"
                    : chunk.fd === 0
                      ? "console-echo"
                      : ""
                }
              >
                {chunk.text}
              </span>
            ))}
          </pre>
          <form class="console-input" onSubmit={sendInput}>
            <label for="rexx-input">
              {waiting ? "Input requested" : "Input"}
            </label>
            <input
              id="rexx-input"
              ref={inputRef}
              value={input}
              disabled={!ready || eof}
              onInput={(event) => setInput(event.currentTarget.value)}
              autoComplete="off"
              aria-label="Rexx input"
            />
            <button type="submit" disabled={!ready || eof}>
              Send
            </button>
            <button
              type="button"
              disabled={!ready || eof}
              onClick={() => {
              const own = generation.current;
                setEof(true);
                void process.current
                  ?.endInput()
                .catch((error) => {if (own === generation.current) stop(String(error));});
              }}
            >
              End input
            </button>
          </form>
        </>
      ) : null}
    </div>
  );
}
