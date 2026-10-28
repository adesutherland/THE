import { useEffect, useRef, useState } from "preact/hooks";
import { browserWasm } from "./transport";
import type { Snapshot } from "./types";
export type ToolSession = {
  type: "tools";
  version: number;
  job: {
    id: number;
    sourceId: number;
    revision: number;
    running: boolean;
    waiting: boolean;
    eof: boolean;
    state: string;
    filename: string;
    tool: string;
  };
  visible: boolean;
  omitted: boolean;
  chunks: { fd: number; text: string }[];
  catalog: {
    id: string;
    label: string;
    role: string;
    image: string;
    extensions: string;
    available: boolean;
  }[];
};
// Presentation only: THE owns source capture, registry, job identity and console.
export function RexxConsole({
  send,
  session,
  snapshot,
}: {
  send: (message: Record<string, unknown>) => number | undefined;
  session: ToolSession;
  snapshot: Snapshot | null;
}) {
  const [height, setHeight] = useState(270),
    [collapsed, setCollapsed] = useState(!session.visible),
    [args, setArgs] = useState(""),
    [input, setInput] = useState(""),
    [tool, setTool] = useState("classic-rexx"),
    [image, setImage] = useState("");
  const importer = useRef<HTMLInputElement>(null);
  const log = useRef<HTMLPreElement>(null),
    entry = useRef<HTMLInputElement>(null),
    job = session.job;
  const act = (action: string, text = "", selected = tool) =>
    send({ type: "tool", action, tool: selected, text, arguments: args });
  useEffect(() => {
    if (session.visible) setCollapsed(false);
  }, [session.visible, job.id]);
  useEffect(() => {
    if (log.current) log.current.scrollTop = log.current.scrollHeight;
  }, [session.version]);
  useEffect(() => {
    if (job.waiting) entry.current?.focus();
  }, [job.waiting]);
  const changed =
    snapshot?.source &&
    job.sourceId &&
    (snapshot.source.id !== job.sourceId ||
      snapshot.source.revision !== job.revision);
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
          tabIndex={0}
          aria-valuenow={height}
          onKeyDown={(e) => {
            if (e.key === "ArrowUp" || e.key === "ArrowDown") {
              e.preventDefault();
              setHeight((h) =>
                Math.max(
                  150,
                  Math.min(
                    window.innerHeight - 250,
                    h + (e.key === "ArrowUp" ? 20 : -20),
                  ),
                ),
              );
            }
          }}
          onPointerDown={(e) => {
            const el = e.currentTarget,
              start = e.clientY,
              previous = height;
            el.setPointerCapture(e.pointerId);
            el.onpointermove = (m) =>
              setHeight(
                Math.max(
                  150,
                  Math.min(
                    window.innerHeight - 250,
                    previous + start - m.clientY,
                  ),
                ),
              );
            el.onpointerup = () => {
              el.onpointermove = null;
              el.onpointerup = null;
            };
          }}
        />
      ) : null}
      <div class="console-heading">
        <strong>Console</strong>
        <select
          aria-label="Run tool"
          value={tool}
          disabled={job.running}
          onChange={(e) => setTool(e.currentTarget.value)}
        >
          {session.catalog
            .filter((t) => ["runner", "program"].includes(t.role))
            .map((t) => (
              <option value={t.id} disabled={!t.available}>
                {t.label}
              </option>
            ))}
        </select>
        <button
          type="button"
          onClick={() => act(tool === "rxbin" ? "rxbin" : "run", image)}
          disabled={
            job.running ||
            !!snapshot?.panel ||
            !session.catalog.find((t) => t.id === tool)?.available
          }
        >
          Run {tool === "classic-rexx" ? "Rexx" : "RXBIN"}
        </button>
        <button
          type="button"
          onClick={() => act("stop")}
          disabled={!job.running}
        >
          Stop
        </button>
        <button type="button" onClick={() => act("clear")}>
          Clear
        </button>
        <span class="run-state" role="status">
          {job.state}
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
            <span>
              {job.filename ||
                "Run the current buffer, including unsaved edits."}
              {changed ? " · buffer changed" : ""}
              {session.omitted ? " · older output omitted" : ""}
            </span>
            <label>
              Arguments
              <textarea
                rows={1}
                aria-label="Rexx arguments"
                placeholder="One argument per line"
                value={args}
                disabled={job.running}
                onInput={(e) => setArgs(e.currentTarget.value)}
              />
            </label>
          </div>
          {tool === "rxbin" ? (
            <label>
              RXBIN workspace path{" "}
              <input
                value={image}
                aria-label="RXBIN path"
                onInput={(e) => setImage(e.currentTarget.value)}
              />
            </label>
          ) : null}
          {browserWasm ? (
            <>
              <input
                ref={importer}
                type="file"
                accept=".rxbin"
                hidden
                onChange={async (e) => {
                  const file = e.currentTarget.files?.[0];
                  if (!file) return;
                  const reader = new FileReader();
                  reader.onload = () => {
                    send({
                      type: "tool-import",
                      name: file.name,
                      bytes: String(reader.result).split(",")[1],
                    });
                    setImage("/workspace/" + file.name);
                    setTool("rxbin");
                  };
                  reader.readAsDataURL(file);
                  e.currentTarget.value = "";
                }}
              />
              <button onClick={() => importer.current?.click()}>
                Import RXBIN
              </button>
            </>
          ) : null}
          <div class="console-heading">
            <button onClick={() => act("profile")} disabled={job.running}>
              Edit profile
            </button>
            <button
              onClick={() => act("build")}
              disabled={
                job.running ||
                !session.catalog.find((t) => t.id === "profile-compiler")
                  ?.available
              }
            >
              Build profile
            </button>
            <button onClick={() => act("activate")} disabled={job.running}>
              Activate profile
            </button>
            <span>Source profiles use cREXX Level B.</span>
          </div>
          <pre
            class="console-output"
            aria-label="Rexx output"
            ref={log}
            tabIndex={0}
          >
            {session.chunks.map((c, i) => (
              <span
                key={i}
                class={
                  c.fd === 2
                    ? "console-stderr"
                    : c.fd === 0
                      ? "console-echo"
                      : ""
                }
              >
                {c.text}
              </span>
            ))}
          </pre>
          <form
            class="console-input"
            onSubmit={(e) => {
              e.preventDefault();
              act("input", input);
              setInput("");
            }}
          >
            <label for="rexx-input">
              {job.waiting ? "Input requested" : "Input"}
            </label>
            <input
              id="rexx-input"
              ref={entry}
              value={input}
              onInput={(e) => setInput(e.currentTarget.value)}
              disabled={!job.running || job.eof}
              autoComplete="off"
              aria-label="Rexx input"
            />
            <button type="submit" disabled={!job.running || job.eof}>
              Send
            </button>
            <button
              type="button"
              onClick={() => act("eof")}
              disabled={!job.running || job.eof}
            >
              End input
            </button>
          </form>
        </>
      ) : null}
    </div>
  );
}
