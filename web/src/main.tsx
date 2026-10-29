import { browserWasm, createEditorTransport, type EditorTransport } from "./transport";
import { render } from "preact";
import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "preact/hooks";
import {
  AlertTriangle,
  ArrowUp,
  Check,
  FileText,
  Folder,
  FolderOpen,
  RefreshCw,
  Save,
  Undo2,
  X,
  Menu,
} from "lucide-preact";
import type { DriverMessage, FileListMessage, FrontendActionDefinition, ScreenRow, Snapshot, StyleRun } from "./types";
import "./styles.css";
import {RexxConsole, type ToolSession} from './rexx-console';
import { consoleColourVariables, renderColour as panelColour } from "./colours";

const keyNames: Record<string, string> = {
  ArrowLeft: "left",
  ArrowRight: "right",
  ArrowUp: "up",
  ArrowDown: "down",
  Home: "home",
  End: "end",
  PageUp: "pageup",
  PageDown: "pagedown",
  Enter: "enter",
  Escape: "esc",
  Tab: "tab",
  Backspace: "backspace",
  Delete: "delete",
  Insert: "insert",
};

function basename(path: string): string {
  const normalized = path.replaceAll("\\", "/");
  return normalized.slice(normalized.lastIndexOf("/") + 1) || path;
}

// Keep a profile's key-guide text intact even when the editor has few columns.
// Other reserved rows retain their normal editor positions.
function keyHintsForRow(row: ScreenRow): { key: string; label: string }[] {
  if (row.role !== "reserved") return [];
  const text = row.t.trim();
  // A modified shortcut is a separate hint, never part of the preceding
  // function key's label. Keep the profile's spelling and labels unchanged.
  const key = "(?:(?:Ctrl|Control|Alt|Meta|Shift)[+-])+(?:F(?:[1-9]|1[0-9]|2[0-4])|[A-Za-z0-9])|F(?:[1-9]|1[0-9]|2[0-4])|Esc";
  const matches = [...text.matchAll(new RegExp(`(${key})\\s+(.+?)(?=\\s+(?:${key})\\s+|$)`, "g"))];
  if (matches.length < 2 || matches[0].index !== 0) return [];
  return matches.map((match) => ({ key: match[1], label: match[2] }));
}

function styledText(text: string, styles: StyleRun[] | undefined) {
  if (!styles?.length) return text.replaceAll("\t", "→");

  const characters = Array.from(text, (character) => character === "\t" ? "→" : character);
  const segments: preact.ComponentChildren[] = [];
  let offset = 0;

  for (const [start, length, style] of [...styles].sort((a, b) => a[0] - b[0])) {
    if (start > offset) segments.push(characters.slice(offset, start).join(""));
    const end = Math.min(characters.length, start + length);
    if (end > start) {
      segments.push(
        <span class={`syntax syntax-${style}`} key={`${start}-${style}`}>
          {characters.slice(start, end).join("")}
        </span>,
      );
    }
    offset = Math.max(offset, end);
  }
  if (offset < characters.length) segments.push(characters.slice(offset).join(""));
  return segments;
}

function pointerCell(event: MouseEvent, startCell: number): number {
  const target = event.currentTarget as HTMLElement;
  const rect = target.getBoundingClientRect();
  const style = getComputedStyle(target);
  const padding = Number.parseFloat(style.paddingLeft) || 0;
  const probe = document.createElement("span");
  probe.className = "cell-probe";
  probe.textContent = "0000000000";
  target.appendChild(probe);
  const width = probe.getBoundingClientRect().width / 10;
  probe.remove();
  return startCell + Math.max(0, Math.floor((event.clientX - rect.left - padding) / width));
}

type DockEntry = { id: number; kind: "command" | "response" | "error"; text: string };

function PaneSplitter({ label, orientation, value, minimum, maximum, reverse = false, consoleDivider = false, onChange }: {
  label: string;
  orientation: "horizontal" | "vertical";
  value: number;
  minimum: number;
  maximum: number;
  reverse?: boolean;
  consoleDivider?: boolean;
  onChange: (value: number) => void;
}) {
  const clamp = (next: number) => Math.max(minimum, Math.min(maximum, next));
  return <div class={`pane-splitter splitter-${orientation} ${consoleDivider ? "console-divider" : ""}`} role="separator" tabIndex={0}
    aria-label={label} aria-orientation={orientation} aria-valuemin={minimum}
    aria-valuemax={maximum} aria-valuenow={Math.round(value)}
    onKeyDown={(event) => {
      const previous = orientation === "vertical" ? "ArrowLeft" : "ArrowUp";
      const next = orientation === "vertical" ? "ArrowRight" : "ArrowDown";
      if (![previous, next, "Home", "End"].includes(event.key)) return;
      event.preventDefault();
      onChange(event.key === "Home" ? minimum : event.key === "End" ? maximum
        : clamp(value + (event.key === previous ? -20 : 20) * (reverse ? -1 : 1)));
    }}
    onPointerDown={(event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      const element = event.currentTarget;
      const start = orientation === "vertical" ? event.clientX : event.clientY;
      const previous = value;
      element.setPointerCapture(event.pointerId);
      element.onpointermove = (move) => onChange(clamp(previous +
        ((orientation === "vertical" ? move.clientX : move.clientY) - start) * (reverse ? -1 : 1)));
      const finish = () => {
        element.onpointermove = null;
        element.onpointerup = null;
        element.onpointercancel = null;
        element.onlostpointercapture = null;
      };
      element.onpointerup = finish;
      element.onpointercancel = finish;
      element.onlostpointercapture = finish;
    }} />;
}

function App() {
  const [snapshot, setSnapshot] = useState<Snapshot | null>(null);
  const [connection, setConnection] = useState<"connecting" | "online" | "reconnecting" | "offline">("connecting");
  const [lastError, setLastError] = useState("");
  const [storageState, setStorageState] = useState("");
  const [toolSession,setToolSession]=useState<ToolSession|null>(null);
  const [command, setCommand] = useState("");
  const [commandFocused, setCommandFocused] = useState(false);
  const [actions, setActions] = useState<FrontendActionDefinition[]>([]);
  const [openMenu, setOpenMenu] = useState<"application" | "about" | null>(null);
  const [diagnosticsOpen, setDiagnosticsOpen] = useState(false);
  const [consoleOpen, setConsoleOpen] = useState(false);
  const [mobilePane, setMobilePane] = useState<"files" | "diagnostics" | "console">("files");
  const [filesWidth, setFilesWidth] = useState(216);
  const [auxWidth, setAuxWidth] = useState(400);
  const [diagnosticsHeight, setDiagnosticsHeight] = useState(190);
  const [windowSize, setWindowSize] = useState({ width: window.innerWidth, height: window.innerHeight });
  const [commandHistoryOpen, setCommandHistoryOpen] = useState(false);
  const [dockEntries, setDockEntries] = useState<DockEntry[]>([]);
  const [pathAction, setPathAction] = useState<FrontendActionDefinition | null>(null);
  const [pathValue, setPathValue] = useState("");
  const [explorerOpen, setExplorerOpen] = useState(false);
  const [files, setFiles] = useState<FileListMessage | null>(null);
  const socketRef = useRef<EditorTransport | null>(null);
  const inputRef = useRef<HTMLTextAreaElement | null>(null);
  const commandInputRef = useRef<HTMLInputElement | null>(null);
  const nextId = useRef(1);
  const viewportRef = useRef<HTMLDivElement | null>(null);
  const gridRef = useRef<HTMLDivElement | null>(null);
  const cellProbeRef = useRef<HTMLSpanElement | null>(null);
  const snapshotRef = useRef<Snapshot | null>(null);
  const lastFileFocusRef = useRef<Snapshot["focus"] | null>(null);
  const viewportSizeRef = useRef("");
  const messageSerialRef = useRef<number | null>(null);
  const dockIdRef = useRef(0);
  const menuRef = useRef<HTMLDivElement | null>(null);
  const menuButtonRef = useRef<HTMLButtonElement | null>(null);
  const menuReturnRef = useRef<HTMLElement | null>(null);
  const responseLogRef = useRef<HTMLDivElement | null>(null);
  const followingResponsesRef = useRef(true);
  const lastShownJob = useRef(0);

  const appendDockEntry = useCallback((kind: DockEntry["kind"], text: string) => {
    if (!text) return;
    const entry = { id: ++dockIdRef.current, kind, text };
    setDockEntries((entries) => [...entries.slice(-199), entry]);
  }, []);

  const send = useCallback((message: Record<string, unknown>) => {
    const socket = socketRef.current;
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    const id = nextId.current++;
    socket.send(JSON.stringify({ v: 1, id, ...message }));
    return id;
  }, []);

  const focusEditor = useCallback(() => inputRef.current?.focus(), []);

  useEffect(() => {
    const token = new URLSearchParams(window.location.search).get("token") ?? "";
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const url = `${protocol}//${window.location.host}/ws?token=${encodeURIComponent(token)}`;
    let disposed = false;
    let pageHidden = false;
    let retryTimer: number | undefined;
    let retryAttempt = 0;

    const connect = () => {
      if (disposed || pageHidden) return;
      setConnection(retryAttempt === 0 ? "connecting" : "reconnecting");
      const socket = createEditorTransport(url);
      socketRef.current = socket;

      socket.onopen = () => {
        if (disposed || socketRef.current !== socket) return;
        retryAttempt = 0;
        viewportSizeRef.current = "";
        setConnection("online");
        setLastError("");
        window.setTimeout(focusEditor, 0);
      };
      socket.onclose = () => {
        if (disposed || socketRef.current !== socket) return;
        socketRef.current = null;
        if (browserWasm) {
          setConnection("offline");
          setLastError("Browser editor closed. Reload to open your workspace.");
          return;
        }
        retryAttempt = Math.min(retryAttempt + 1, 5);
        setConnection("reconnecting");
        setLastError("Web driver connection interrupted");
        const delay = Math.min(500 * (2 ** (retryAttempt - 1)), 5000);
        retryTimer = window.setTimeout(connect, delay);
      };
      socket.onerror = () => {
        if (!disposed && socketRef.current === socket) {
          if (browserWasm) setConnection("offline");
          setLastError(browserWasm ? "Browser editor stopped. Reload to reopen saved files." : "Web driver connection interrupted");
        }
      };
      socket.onmessage = (event) => {
        try {
          const message = (typeof event.data === 'string' ? JSON.parse(event.data) : event.data) as Snapshot | DriverMessage;
          if ("type" in message && message.type === "tools") {
            setToolSession(message as unknown as ToolSession);

          } else if ("type" in message && message.type === "storage") {
            const storage = message as DriverMessage & { state: string };
            setStorageState(storage.state === "error" ? storage.message ?? "Storage error" : storage.state === "saving" ? "Saving browser workspace…" : "Saved to browser workspace");
          } else if ("type" in message && message.type === "fatal") {
            setConnection("offline");
            setLastError(message.message ?? "Browser editor could not start");
          } else if ("mode" in message && "screen_rows" in message) {
            const nextSnapshot = message as Snapshot;
            if (typeof nextSnapshot.last_message_serial === "number") {
              if (messageSerialRef.current !== null && messageSerialRef.current !== nextSnapshot.last_message_serial)
                appendDockEntry("response", nextSnapshot.last_message ?? "");
              messageSerialRef.current = nextSnapshot.last_message_serial;
            }
            if (snapshotRef.current?.buffer?.path !== nextSnapshot.buffer?.path) lastFileFocusRef.current = null;
            if (nextSnapshot.focus.zone === "filearea") lastFileFocusRef.current = nextSnapshot.focus;
            snapshotRef.current = nextSnapshot;
            setSnapshot(nextSnapshot);
            if (nextSnapshot.focus.zone === "command") setCommand(nextSnapshot.command);
            setLastError("");
          } else if ("type" in message && message.type === "actions" && message.actions) {
            setActions(message.actions);
          } else if ("type" in message && message.type === "files") {
            setFiles(message as FileListMessage);
          } else if ("type" in message && message.type === "error") {
            setLastError(message.message ?? "Driver rejected the input");
            appendDockEntry("error", message.message ?? "Driver rejected the input");
          }
        } catch {
          setLastError("The web driver returned invalid JSON");
        }
      };
    };

    const close = () => {
      if (retryTimer !== undefined) window.clearTimeout(retryTimer);
      retryTimer = undefined;
      const socket = socketRef.current;
      socketRef.current = null;
      if (socket) {
        socket.onclose = null;
        socket.close();
      }
    };
    // Navigating away can cache the page without unmounting the application.
    // Release control so READ cancels and a returning page can reconnect.
    const hide = () => {
      pageHidden = true;
      close();
    };
    const show = () => {
      if (!pageHidden) return;
      pageHidden = false;
      connect();
    };
    const beforeUnload = (event: BeforeUnloadEvent) => {
      if (browserWasm && snapshotRef.current?.buffers.some((buffer) => buffer.dirty)) {
        event.preventDefault();
        event.returnValue = "";
      }
    };
    window.addEventListener("pagehide", hide);
    window.addEventListener("pageshow", show);
    window.addEventListener("beforeunload", beforeUnload);
    connect();
    return () => {
      disposed = true;
      window.removeEventListener("pagehide", hide);
      window.removeEventListener("pageshow", show);
      window.removeEventListener("beforeunload", beforeUnload);
      close();
    };
  }, [appendDockEntry, focusEditor]);

  useEffect(() => {
    const resized = () => setWindowSize({ width: window.innerWidth, height: window.innerHeight });
    window.addEventListener("resize", resized);
    return () => window.removeEventListener("resize", resized);
  }, []);

  useEffect(() => {
    const closeMenu = (event: PointerEvent) => {
      if (!menuRef.current?.contains(event.target as Node)) setOpenMenu(null);
    };
    document.addEventListener("pointerdown", closeMenu);
    return () => {
      document.removeEventListener("pointerdown", closeMenu);
    };
  }, []);

  useLayoutEffect(() => {
    // Each view replaces the previous buttons. Give it a focused action while
    // retaining the original invoking control for Escape.
    if (openMenu) menuRef.current?.querySelector<HTMLButtonElement>(".menu-popover button:not(:disabled)")?.focus();
  }, [openMenu]);

  useEffect(() => {
    if (toolSession?.job.id && toolSession.job.id !== lastShownJob.current) {
      lastShownJob.current = toolSession.job.id;
      setConsoleOpen(true);
      setMobilePane("console");
    }
  }, [toolSession?.job.id]);

  useEffect(() => {
    if (snapshot?.diagnostics?.length) setDiagnosticsOpen(true);
  }, [snapshot?.buffer?.path, !!snapshot?.diagnostics?.length]);

  useEffect(() => {
    if (followingResponsesRef.current && responseLogRef.current)
      responseLogRef.current.scrollTop = responseLogRef.current.scrollHeight;
  }, [dockEntries[dockEntries.length - 1]?.id, commandHistoryOpen]);

  const runCommand = useCallback(
    (value: string) => {
      const trimmed = value.trim();
      if (!trimmed) return;
      if (send({ type: "command", command: trimmed }) === undefined) return;
      appendDockEntry("command", trimmed);
      setCommand("");
      window.setTimeout(focusEditor, 0);
    },
    [appendDockEntry, focusEditor, send],
  );

  const sendAction = useCallback(
    (action: string, argument = "") => {
      send({ type: "action", action, argument });
      setOpenMenu(null);
      window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  const actionById = useMemo(
    () => new Map(actions.map((action) => [action.id, action])),
    [actions],
  );

  const requestFiles = useCallback(
    (root = files?.root ?? 0, path = files?.path ?? "") => {
      send({ type: "files", root, path });
    },
    [files?.path, files?.root, send],
  );

  const requestAction = useCallback(
    (id: string, argument?: string) => {
      const action = actionById.get(id);
      if (!action) return;
      setOpenMenu(null);
      if (id === "file.open" && argument === undefined) {
        setExplorerOpen(true);
        setMobilePane("files");
        requestFiles();
        return;
      }
      if (action.requires_argument && argument === undefined) {
        setPathValue("");
        setPathAction(action);
        return;
      }
      sendAction(id, argument ?? "");
    },
    [actionById, requestFiles, sendAction],
  );

  const keyHintRows = useMemo(() => snapshot?.panel ? [] :
    (snapshot?.screen_rows ?? []).map((row) => ({ row, hints: keyHintsForRow(row) }))
      .filter((entry) => entry.hints.length > 0), [snapshot?.screen_rows, !!snapshot?.panel]);
  const keyHintRowIds = new Set(keyHintRows.map(({ row }) => row.r));
  const keyHints = keyHintRows.flatMap(({ hints }) => hints);

  useEffect(() => {
    const viewport = viewportRef.current;
    if (connection !== "online" || !viewport) return;
    let animation = 0;
    const measure = () => {
      cancelAnimationFrame(animation);
      animation = requestAnimationFrame(() => {
        const current = snapshotRef.current;
        const grid = gridRef.current;
        const probe = cellProbeRef.current;
        if (!current || current.panel || !grid || !probe) return;
        const style = getComputedStyle(grid);
        const cellWidth = probe.getBoundingClientRect().width / 10;
        const lineHeight = Number.parseFloat(style.lineHeight);
        if (!cellWidth || !lineHeight) return;
        const padding = Number.parseFloat(style.paddingTop) + Number.parseFloat(style.paddingBottom);
        // The browser draws only the file area; the model also reserves its
        // command/status/heading rows. Preserve that overhead on resize.
        const overhead = Math.max(0, current.viewport?.reserved_rows ?? 4);
        // Key-guide rows are shown in a wrapping footer, so their model rows
        // take no space in this grid. Fill the recovered rows with file text.
        const hintRows = current.screen_rows.filter((row) => keyHintsForRow(row).length > 0).length;
        const rows = Math.max(12, Math.min(256, Math.floor((viewport.clientHeight - padding) / lineHeight) + overhead + hintRows));
        const cols = Math.max(40, Math.min(1000, Math.floor((viewport.clientWidth - 20) / cellWidth)));
        const size = `${rows}:${cols}`;
        if (size !== viewportSizeRef.current || current.viewport?.rows !== rows || current.viewport?.cols !== cols) {
          viewportSizeRef.current = size;
          send({ type: "resize", rows, cols });
        }
      });
    };
    const observer = new ResizeObserver(measure);
    observer.observe(viewport);
    measure();
    return () => { observer.disconnect(); cancelAnimationFrame(animation); };
  }, [connection, send, snapshot?.buffer?.path, !!snapshot?.panel, keyHintRows.length]);

  const returnToFile = useCallback(() => {
    const current = snapshotRef.current?.focus;
    const saved = lastFileFocusRef.current;
    const target = current && (current.zone === "filearea" || current.zone === "prefix") ? current : saved;
    if (target) {
      const cell = target.zone === "prefix" ? (saved?.line === target.line ? saved.cell : 0) : target.cell;
      send({ type: "hit", target: "filearea", line: target.line, row: target.row, cell });
    }
    focusEditor();
  }, [focusEditor, send]);

  const onWorkbenchKeyDown = useCallback((event: KeyboardEvent) => {
    // Panels and path dialogs own their keys, including function keys.
    if (snapshot?.panel || pathAction) return;
    if (event.key === "F10" && !event.ctrlKey && !event.altKey && !event.metaKey && !event.shiftKey) {
      event.preventDefault();
      event.stopPropagation();
      const invokingControl = document.activeElement;
      if ((!openMenu || !menuRef.current?.contains(invokingControl)) && invokingControl instanceof HTMLElement)
        menuReturnRef.current = invokingControl;
      setOpenMenu("application");
      // The effect handles newly mounted menus; an already open menu also
      // needs a predictable starting point when F10 is pressed again.
      if (openMenu === "application")
        menuRef.current?.querySelector<HTMLButtonElement>(".menu-popover button:not(:disabled)")?.focus();
      return;
    }
    if (!openMenu) return;
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      setOpenMenu(null);
      const invokingControl = menuReturnRef.current;
      if (invokingControl?.isConnected && !invokingControl.matches(":disabled") && invokingControl.getClientRects().length)
        invokingControl.focus();
      else menuButtonRef.current?.focus();
      return;
    }
    const menu = menuRef.current?.querySelector<HTMLElement>(".menu-popover");
    if (!menuRef.current?.contains(event.target as Node) || !menu) return;
    const items = [...menu.querySelectorAll<HTMLButtonElement>("button:not(:disabled)")];
    if (!items.length) return;
    const current = items.indexOf(document.activeElement as HTMLButtonElement);
    let next: number;
    if (event.key === "ArrowDown") next = (current + 1) % items.length;
    else if (event.key === "ArrowUp") next = (current <= 0 ? items.length : current) - 1;
    else if (event.key === "Home") next = 0;
    else if (event.key === "End") next = items.length - 1;
    else if (event.key === "Enter" && current >= 0) {
      event.preventDefault();
      event.stopPropagation();
      items[current].click();
      return;
    } else return; // Tab retains ordinary browser traversal.
    event.preventDefault();
    event.stopPropagation();
    items[next]?.focus();
  }, [openMenu, pathAction, snapshot?.panel]);

  const onEditorKeyDown = useCallback(
    (event: KeyboardEvent) => {
      if (snapshot?.panel && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        return;
      }
      const commandModifier = event.ctrlKey || event.metaKey;
      if (!snapshot?.panel && (event.key === "Escape" || event.key === "F6"
          || (commandModifier && event.key.toLowerCase() === "l"))) {
        event.preventDefault();
        commandInputRef.current?.focus();
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "s") {
        event.preventDefault();
        requestAction("file.save");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "o") {
        event.preventDefault();
        requestAction("file.open");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "n") {
        event.preventDefault();
        requestAction("file.create");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "w") {
        event.preventDefault();
        requestAction("file.close");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "z") {
        event.preventDefault();
        if (!event.shiftKey) requestAction("edit.undo");
        return;
      }
      if (commandModifier && ["f", "g"].includes(event.key.toLowerCase())) {
        event.preventDefault();
        send({ type: "key", key: `C-${event.key.toLowerCase()}` });
        return;
      }
      let key = /^F([1-9]|1[0-9]|2[0-4])$/.test(event.key) ? event.key.toLowerCase() : event.shiftKey && event.key === "Tab" ? "backtab" : keyNames[event.key];
      if (event.shiftKey && key && ["left", "right", "up", "down", "home", "end"].includes(key)) key = `shift-${key}`;
      if (key) {
        event.preventDefault();
        send({ type: "key", key });
      }
    },
    [requestAction, send, snapshot?.panel],
  );

  const rows = snapshot?.screen_rows ?? [];
  const currentBuffer = snapshot?.buffers.find((buffer) => buffer.current);
  const title = currentBuffer ? basename(currentBuffer.path) : "CRXTHE";
  const cursorRow = useMemo(
    () => rows.find((row) => row.r === snapshot?.focus.row && row.line === snapshot?.focus.line),
    [rows, snapshot?.focus.line, snapshot?.focus.row],
  );
  const prefixCols = snapshot?.viewport?.prefix_cols ?? 7;
  const prefixGap = snapshot?.viewport?.prefix_gap ?? 1;
  const menus = useMemo(() => {
    const result = new Map<string, FrontendActionDefinition[]>();
    for (const action of actions) {
      if (!action.menu) continue;
      const entries = result.get(action.menu) ?? [];
      entries.push(action);
      result.set(action.menu, entries);
    }
    return [...result.entries()];
  }, [actions]);

  const hitRow = useCallback(
    (event: MouseEvent, row: ScreenRow, target: "filearea" | "prefix") => {
      const cell = pointerCell(event, target === "filearea" ? row.sc : 0);
      send({ type: "hit", target, line: row.line, row: row.r, cell });
      window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  const toolsAvailable = !!toolSession?.catalog.some((tool) => tool.available);
  const maximumFilesWidth = Math.max(180, Math.min(400, Math.floor(windowSize.width * .25)));
  const maximumAuxWidth = Math.max(300, Math.min(650, Math.floor(windowSize.width * .36)));
  const maximumDiagnosticsHeight = Math.max(96, Math.min(440, Math.floor((windowSize.height - 120) * .35)));
  const showAux = diagnosticsOpen || (consoleOpen && toolsAvailable);
  const activeMobilePane = mobilePane === "files" && explorerOpen ? "files"
    : mobilePane === "diagnostics" && diagnosticsOpen ? "diagnostics"
    : mobilePane === "console" && consoleOpen && toolsAvailable ? "console"
    : explorerOpen ? "files" : diagnosticsOpen ? "diagnostics" : "console";
  const latestResponse = [...dockEntries].reverse().find((entry) => entry.kind !== "command");
  const toggleFiles = () => {
    if (window.innerWidth >= 1100 || activeMobilePane === "files") setExplorerOpen(!explorerOpen);
    else setExplorerOpen(true);
    setMobilePane("files");
    if (!explorerOpen) requestFiles();
  };

  return (
    <main class="app-shell" onKeyDownCapture={onWorkbenchKeyDown} style={consoleColourVariables(snapshot?.console_colours)}>
      <header class="toolbar">
        <div class="product-mark">CRXTHE</div>
        <div class="menu-root" ref={menuRef}>
          <button type="button" ref={menuButtonRef} class={`menu-trigger ${openMenu ? "menu-trigger-open" : ""}`}
            aria-expanded={!!openMenu} aria-controls="application-menu"
            onClick={() => { menuReturnRef.current = menuButtonRef.current; setOpenMenu(openMenu ? null : "application"); }}><Menu size={16} /> Menu</button>
          {openMenu ? <nav id="application-menu" class={`menu-popover ${openMenu === "about" ? "about-menu" : ""}`} aria-label={openMenu === "about" ? "About CRXTHE" : "Application menu"}>
            {openMenu === "about" ? <>
              <button type="button" onClick={() => setOpenMenu("application")}>← Back to Menu</button>
              <section aria-labelledby="about-title">
                <h2 id="about-title">CRXTHE</h2>
                <p>A programmable editor based on THE, with line prefixes, editor commands and Rexx macros.</p>
                <p>Use the THE command field for editor commands. The Program console has separate input and output for the program you run.</p>
                <p>Help describes editor commands. Key definitions shows the active profile’s bindings.</p>
              </section>
            </> : <>
            {menus.map(([menu, entries]) => <section key={menu} aria-label={menu}>
              <h2>{menu}</h2>
              {entries.map((action) => <button type="button" onClick={() => requestAction(action.id)}>
                <span>{action.label}</span>
                <kbd>{action.id === "file.create" ? "Ctrl+N" : action.id === "file.open" ? "Ctrl+O" : action.id === "file.save" ? "Ctrl+S" : action.id === "file.close" ? "Ctrl+W" : action.id === "edit.undo" ? "Ctrl+Z" : ""}</kbd>
              </button>)}
            </section>)}
            <section aria-label="Workbench"><h2>Workbench</h2>
              <button type="button" onClick={() => { toggleFiles(); setOpenMenu(null); }}>Files</button>
              <button type="button" onClick={() => { setDiagnosticsOpen(true); setMobilePane("diagnostics"); setOpenMenu(null); }}>Diagnostics</button>
              {toolsAvailable ? <button type="button" onClick={() => { setConsoleOpen(true); setMobilePane("console"); setOpenMenu(null); }}>Program console</button> : null}
              <button type="button" onClick={() => {
                setOpenMenu(null);
                // Keep the existing admitted profile-key route: SHOWKEY is
                // deliberately unavailable as a typed restricted command.
                send({ type: "key", key: "f10" }); focusEditor();
              }}>Key definitions</button>
            </section>
            <section aria-label="About"><button type="button" onClick={() => setOpenMenu("about")}>About CRXTHE</button></section>
            </>}
          </nav> : null}
        </div>
        <div class="document-title" title={currentBuffer?.path}>{title}</div>
        <div class="toolbar-actions">
          <button class="icon-button" type="button" title="Open" aria-label="Open" onClick={() => requestAction("file.open")}>
            <FolderOpen size={17} />
          </button>
          <button class="icon-button" type="button" title="Save" aria-label="Save" onClick={() => requestAction("file.save")}>
            <Save size={17} />
          </button>
          <button class="icon-button" type="button" title="Undo" aria-label="Undo" onClick={() => requestAction("edit.undo")}>
            <Undo2 size={17} />
          </button>
          <button class="icon-button" type="button" title="Refresh" aria-label="Refresh" onClick={() => send({ type: "snapshot" })}>
            <RefreshCw size={17} />
          </button>
        </div>
        <div class={`connection connection-${connection}`} title={`Web driver: ${connection}`}>
          {connection === "online" ? <Check size={14} /> : connection === "reconnecting" ? <RefreshCw size={14} /> : <AlertTriangle size={14} />}
          <span>{connection}</span>
        </div>
      </header>

      <nav class="buffer-tabs" aria-label="Open buffers">
        {snapshot?.buffers.map((buffer) => (
          <button
            type="button"
            class={`buffer-tab ${buffer.current ? "buffer-tab-current" : ""}`}
            title={buffer.path}
            onClick={() => !buffer.current && requestAction("buffer.switch", buffer.path)}
          >
            <span>{basename(buffer.path)}</span>
            {buffer.dirty ? <span class="dirty-mark" aria-label="Modified" /> : null}
          </button>
        ))}
      </nav>

      <div class="workbench">
      <div class="pane-switcher" role="group" aria-label="Workbench panes">
        <button type="button" aria-pressed={explorerOpen} onClick={toggleFiles}>Files</button>
        <button type="button" aria-pressed={diagnosticsOpen} onClick={() => { setDiagnosticsOpen(window.innerWidth < 1100 && activeMobilePane !== "diagnostics" ? true : !diagnosticsOpen); setMobilePane("diagnostics"); }}>Diagnostics <span>{snapshot?.diagnostics?.length ?? 0}</span></button>
        {toolsAvailable ? <button type="button" aria-pressed={consoleOpen} onClick={() => { setConsoleOpen(window.innerWidth < 1100 && activeMobilePane !== "console" ? true : !consoleOpen); setMobilePane("console"); }}>
          Program console{toolSession?.job.waiting ? " · Input requested" : toolSession?.job.running ? " · Running" : ""}
        </button> : null}
      </div>
      <section class={`workspace mobile-pane-${activeMobilePane} ${explorerOpen ? "files-open" : ""} ${showAux ? "aux-open" : ""}`} style={{
        "--files-width": `${Math.min(filesWidth, maximumFilesWidth)}px`, "--aux-width": `${Math.min(auxWidth, maximumAuxWidth)}px`,
        "--diagnostics-height": `${Math.min(diagnosticsHeight, maximumDiagnosticsHeight)}px`,
      }}>
        {explorerOpen ? (
          <aside class="file-explorer" aria-label="Workspace files">
            <div class="explorer-heading">
              <span>Files</span>
              <button class="icon-button" type="button" title="Close files" aria-label="Close files" onClick={() => { setExplorerOpen(false); focusEditor(); }}><X size={16} /></button>
            </div>
            <div class="root-switcher" aria-label="Workspace roots">
              {files?.roots.map((root) => (
                <button type="button" class={root.id === files.root ? "root-current" : ""} onClick={() => requestFiles(root.id, "")}>
                  <Folder size={14} />
                  <span>{root.name}</span>
                  {root.readonly ? <span class="readonly-label">RO</span> : null}
                </button>
              ))}
            </div>
            <div class="explorer-path" title={files?.path || "/"}>
              <button
                class="icon-button"
                type="button"
                title="Parent directory"
                aria-label="Parent directory"
                disabled={!files?.path}
                onClick={() => requestFiles(files?.root ?? 0, (files?.path ?? "").split("/").slice(0, -1).join("/"))}
              >
                <ArrowUp size={15} />
              </button>
              <span>{files?.path || "/"}</span>
              <button class="icon-button" type="button" title="Refresh files" aria-label="Refresh files" onClick={() => requestFiles()}><RefreshCw size={14} /></button>
            </div>
            <div class="file-list">
              {[...(files?.entries ?? [])].sort((left, right) => {
                if (left.type !== right.type) return left.type === "directory" ? -1 : 1;
                return left.name.localeCompare(right.name);
              }).map((entry) => (
                <button
                  type="button"
                  title={entry.path}
                  onClick={() => {
                    if (entry.type === "directory") requestFiles(files?.root ?? 0, entry.path);
                    else {
                      requestAction("file.open", entry.target);
                      setExplorerOpen(false);
                    }
                  }}
                >
                  {entry.type === "directory" ? <Folder size={15} /> : <FileText size={15} />}
                  <span>{entry.name}</span>
                  {entry.readonly ? <span class="readonly-label">RO</span> : null}
                </button>
              ))}
              {files && files.entries.length === 0 ? <div class="empty-directory">No files</div> : null}
            </div>
          </aside>
        ) : null}
        {explorerOpen ? <PaneSplitter label="Resize files pane" orientation="vertical" value={Math.min(filesWidth, maximumFilesWidth)} minimum={180} maximum={maximumFilesWidth} onChange={setFilesWidth} /> : null}
        <div class="editor-pane">
        <div class="editor-scroll" ref={viewportRef} onClick={focusEditor}>
          {snapshot?.panel ? (
            <div class="fullscreen-panel" role="dialog" aria-modal="true" aria-label="Macro panel" style={panelColour(snapshot.panel.colour)}>
              <div class="panel-viewport">
              <div class="panel-surface" style={{ width: `${snapshot.panel.cols}ch`, height: `${snapshot.panel.rows * 21}px` }}>
                {snapshot.panel.fields.map((field, index) => (
                  <span
                    class={`panel-field ${field.editable ? "panel-editable" : "panel-protected"} ${snapshot.panel?.active === index ? "panel-focused" : ""}`}
                    style={{ ...panelColour(field.colour), top: `${field.row * 21}px`, left: `${field.cell}ch`, width: `${field.width}ch` }}
                    onClick={(event) => {
                      if (!field.editable) return;
                      const bounds = event.currentTarget.getBoundingClientRect();
                      const offset = Math.max(0, Math.min(field.width - 1,
                        Math.floor((event.clientX - bounds.left) * field.width / bounds.width)));
                      send({ type: "hit", target: "window", row: field.row, cell: field.cell + offset });
                    }}
                  >
                    {field.text}
                    {snapshot.panel?.active === index ? <span class="logical-cursor" style={{
                      left: `${snapshot.panel.cursor_cell ?? Math.min(snapshot.panel.cursor, field.width - 1)}ch`,
                      width: `${snapshot.panel.cursor_width ?? 1}ch`,
                    }} /> : null}
                  </span>
                ))}
              </div>
              </div>
              <div class="panel-controls">
                <span>Macro input · Tab changes field</span>
                <button type="button" onClick={() => send({ type: "key", key: "enter" })}>Enter</button>
                <select aria-label="Panel function key" onChange={(event) => { if (event.currentTarget.value) send({ type: "key", key: event.currentTarget.value }); event.currentTarget.value = ""; }}>
                  <option value="">Function key…</option>
                  {Array.from({ length: 24 }, (_, i) => <option value={`f${i + 1}`}>PF{i + 1}</option>)}
                  {Array.from({ length: 3 }, (_, i) => <option value={`pa${i + 1}`}>PA{i + 1}</option>)}
                </select>
                <button type="button" onClick={() => send({ type: "key", key: "esc" })}>Cancel</button>
              </div>
            </div>
          ) : null}
          <div class="editor-grid" ref={gridRef} style={{ width: `calc(${snapshot?.viewport?.cols ?? 80}ch + 20px)`, "--prefix-width": `${prefixCols + prefixGap}ch` }}>
            <span class="cell-probe" ref={cellProbeRef} aria-hidden="true">0000000000</span>
            {rows.map((row) => {
              const cursorVisible = cursorRow === row && snapshot?.focus.zone === "filearea" && !commandFocused;
              const prefixCursorVisible = cursorRow === row && snapshot?.focus.zone === "prefix" && !commandFocused;
              const prefixCursorCell = Math.max(0, Math.min((snapshot?.focus.cell ?? 0), prefixCols - 1));
              const cursorCell = Math.max(0, (snapshot?.focus.cell ?? 0) - row.sc);
              return (
                <div class={`screen-row role-${row.role} ${row.cur ? "screen-row-current" : ""} ${keyHintRowIds.has(row.r) ? "key-hint-row" : ""}`} key={`${currentBuffer?.path ?? ""}-${row.r}-${row.line}-${row.role}`}>
                  <button
                    type="button"
                    class={`prefix-cell ${prefixCursorVisible ? "prefix-cell-active" : ""}`}
                    aria-label={`Prefix line ${row.line}`}
                    style={{ paddingRight: `${prefixGap}ch` }}
                    tabIndex={-1}
                    onClick={(event) => hitRow(event, row, "prefix")}
                  >
                    {row.pc || row.p || ""}
                    {prefixCursorVisible ? <span class="logical-cursor prefix-cursor" style={{ left: `${prefixCursorCell}ch` }} /> : null}
                  </button>
                  <div class="line-content" onClick={(event) => hitRow(event, row, "filearea")}>
                    <span class="line-text">{styledText(row.t, row.s)}</span>
                    {cursorVisible ? <span class="logical-cursor" style={{ left: `${cursorCell}ch` }} /> : null}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        <section class="editor-status" aria-label="Editor status and key guide">
          <div class="status-line" aria-label="Editing status" hidden={snapshot?.status_line === null}
            data-focus-zone={snapshot?.focus.zone} data-focus-line={snapshot?.focus.line} data-focus-cell={snapshot?.focus.cell} data-buffer-dirty={snapshot?.buffer?.dirty ?? 0}>
            {typeof snapshot?.status_line === "string" ? <pre class="profile-status">{snapshot.status_line}</pre> : <>
            <span>Ln {snapshot?.focus.line ?? 0}, Col {(snapshot?.focus.cell ?? 0) + 1}</span>
            <span>{snapshot?.focus.zone === "prefix" ? "Prefix" : snapshot?.focus.zone === "command" || commandFocused ? "Command" : "File"}</span>
            <span>{snapshot?.buffer?.dirty ? "Modified" : "Saved"}</span>
            </>}
          </div>
          {browserWasm || (snapshot?.parser && snapshot.parser.state !== "inactive") ? <div class="editor-indicators" aria-label="Workspace and syntax status">
            {browserWasm ? <span class="workspace-storage" role="status">{storageState}</span> : null}
            {snapshot?.parser && snapshot.parser.state !== "inactive" ? <button class="parser-status" title="Restart the highlighter" onClick={() => send({type: "parser-restart"})}>Syntax: {snapshot.parser.state}</button> : null}
          </div> : null}
          {keyHints.length > 0 ? <div class="key-hints" role="group" aria-label="Editor key hints">{keyHints.map((hint, index) => <span class="key-hint" key={`${hint.key}-${index}`}><kbd>{hint.key}</kbd><span>{hint.label}</span></span>)}</div> : null}
        </section>

        <footer class="command-area" aria-label="THE commands and responses">
          <div class="dock-heading"><strong>THE command</strong>
            <button type="button" aria-expanded={commandHistoryOpen} aria-controls="command-responses" onClick={() => setCommandHistoryOpen(!commandHistoryOpen)}>
              {commandHistoryOpen ? "Hide history" : "History"}{dockEntries.length ? ` (${dockEntries.length})` : ""}
            </button>
          </div>
          <div class="latest-response" role="status" aria-live="polite">{lastError || latestResponse?.text || snapshot?.last_message || ""}</div>
          <div id="command-responses" class="command-responses" hidden={!commandHistoryOpen} ref={responseLogRef}
            aria-label="THE command and response history" tabIndex={0}
            onScroll={(event) => { const log = event.currentTarget; followingResponsesRef.current = log.scrollHeight - log.scrollTop - log.clientHeight < 24; }}>
            {dockEntries.length ? dockEntries.map((entry) => <div class={`dock-entry dock-entry-${entry.kind}`} key={entry.id}>
              <span>{entry.kind === "command" ? "Command" : entry.kind === "error" ? "Error" : "Response"}</span><code>{entry.text}</code>
            </div>) : <p>Commands and editor messages appear here.</p>}
          </div>
          {!snapshot?.panel ? <form class="command-form" onSubmit={(event) => { event.preventDefault(); runCommand(command); }}>
            <label class="command-prompt" for="the-command">Command</label>
            <input id="the-command" aria-label="THE command" ref={commandInputRef}
              onFocus={() => setCommandFocused(true)} onBlur={() => setCommandFocused(false)}
              onKeyDown={(event) => {
                if (event.key === "Escape") { event.preventDefault(); focusEditor(); }
                if (event.key === "F6") { event.preventDefault(); returnToFile(); }
              }} value={command} onInput={(event) => setCommand(event.currentTarget.value)} spellcheck={false} autoComplete="off" />
            <button type="submit" disabled={connection !== "online" || !command.trim()}>Execute</button>
          </form> : null}
        </footer>
        </div>

        <div class="mobile-aux-tabs" role="group" aria-label="Choose auxiliary pane">
          <button type="button" aria-pressed={activeMobilePane === "files" && explorerOpen} onClick={() => { setExplorerOpen(true); setMobilePane("files"); requestFiles(); }}>Files</button>
          <button type="button" aria-pressed={activeMobilePane === "diagnostics" && diagnosticsOpen} onClick={() => { setDiagnosticsOpen(true); setMobilePane("diagnostics"); }}>Diagnostics <span>{snapshot?.diagnostics?.length ?? 0}</span></button>
          {toolsAvailable ? <button type="button" aria-pressed={activeMobilePane === "console" && consoleOpen} onClick={() => { setConsoleOpen(true); setMobilePane("console"); }}>Program console{toolSession?.job.waiting ? " · Input requested" : ""}</button> : null}
        </div>
        {showAux ? <PaneSplitter label="Resize auxiliary panes" orientation="vertical" reverse consoleDivider={consoleOpen && toolsAvailable} value={Math.min(auxWidth, maximumAuxWidth)} minimum={300} maximum={maximumAuxWidth} onChange={setAuxWidth} /> : null}
        <div class={`auxiliary-rail ${diagnosticsOpen && consoleOpen && toolsAvailable ? "auxiliary-split" : ""}`} hidden={!showAux}>
        {diagnosticsOpen ? (
          <aside class="diagnostics" aria-label="Diagnostics">
            <div class="panel-heading">Diagnostics <span>{snapshot?.diagnostics?.length ?? 0}</span><button type="button" class="icon-button" aria-label="Close diagnostics" onClick={() => { setDiagnosticsOpen(false); focusEditor(); }}><X size={16} /></button></div>
            {!snapshot?.diagnostics?.length ? <p class="empty-directory">No diagnostics for this buffer.</p> : null}
            {snapshot?.diagnostics?.map((diagnostic) => (
              <button
                type="button"
                class="diagnostic-row"
                onClick={() => {
                  send({ type: "hit", target: "filearea", line: diagnostic.line, row: 0, cell: Math.max(0, diagnostic.column - 1) });
                  focusEditor();
                }}
              >
                <span class={`severity severity-${(diagnostic.severity || "info").toLowerCase()}`}>{diagnostic.severity || "info"}</span>
                <span class="diagnostic-location">{diagnostic.line}:{diagnostic.column}</span>
                <span class="diagnostic-message">{diagnostic.message}</span>
              </button>
            ))}
          </aside>
        ) : null}
        {diagnosticsOpen && consoleOpen && toolsAvailable ? <PaneSplitter label="Resize diagnostics and program console" orientation="horizontal" consoleDivider value={Math.min(diagnosticsHeight, maximumDiagnosticsHeight)} minimum={96} maximum={maximumDiagnosticsHeight} onChange={setDiagnosticsHeight} /> : null}
        {toolsAvailable && toolSession ? <div class="program-console-pane" hidden={!consoleOpen}>
          <RexxConsole send={send} session={toolSession} snapshot={snapshot} onClose={() => { setConsoleOpen(false); focusEditor(); }} focusEditor={focusEditor} />
        </div> : null}
        </div>
      </section>
      </div>

      <textarea
        ref={inputRef}
        class="input-sink"
        aria-label="Editor input"
        onKeyDown={onEditorKeyDown}
        onInput={(event) => {
          const value = event.currentTarget.value;
          if (value) send({ type: "text", text: value });
          event.currentTarget.value = "";
        }}
        autoCapitalize="off"
        autoCorrect="off"
        spellcheck={false}
      />

      {pathAction ? (
        <div class="dialog-backdrop" role="presentation" onMouseDown={(event) => {
          if (event.currentTarget === event.target) setPathAction(null);
        }}>
          <form class="path-dialog" role="dialog" aria-modal="true" aria-labelledby="path-dialog-title" onSubmit={(event) => {
            event.preventDefault();
            const value = pathValue.trim();
            if (!value) return;
            sendAction(pathAction.id, value);
            setPathAction(null);
          }}>
            <div class="dialog-heading">
              <h2 id="path-dialog-title">{pathAction.label} file</h2>
              <button class="icon-button" type="button" aria-label="Close" title="Close" onClick={() => setPathAction(null)}><X size={17} /></button>
            </div>
            <label for="file-path">Path</label>
            <input id="file-path" autoFocus value={pathValue} onInput={(event) => setPathValue(event.currentTarget.value)} spellcheck={false} autoComplete="off" />
            <div class="dialog-actions">
              <button type="button" onClick={() => setPathAction(null)}>Cancel</button>
              <button type="submit" class="primary-command" disabled={!pathValue.trim()}>{pathAction.label}</button>
            </div>
          </form>
        </div>
      ) : null}
    </main>
  );
}

render(<App />, document.getElementById("app")!);
