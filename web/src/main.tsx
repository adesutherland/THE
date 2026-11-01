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
import type { DriverMessage, FileListMessage, FrontendActionDefinition, ScreenRow, Snapshot, StyleRun, WorkbenchActionDefinition, WorkbenchPage, WorkbenchPanelDefinition, WorkbenchPanelState, WorkbenchPanelStateMessage, WorkbenchLogPage, WorkbenchLogRecord, WorkbenchDocumentPage, WorkbenchFormPage, WorkbenchFileFieldPage } from "./types";
import { WorkbenchPageCollector, displayLogText, type WorkbenchCatalog } from "./workbench";
import "./styles.css";
import { RexxConsole } from "./rexx-console";
import { CurrentFileActions, ToolsProfiles, fileActionReason } from "./file-tools";
import type { FileToolAction, ToolSession } from "./tool-session";
import { consoleColourVariables, diagnosticColourVariables, renderColour as panelColour } from "./colours";
import { HighlightPresentation } from "./highlight-presentation";
import { WorkbenchForms, type FormView } from "./workbench-forms";
import { NamedFormPane, type FormViewport } from "./form-pane";
import { WorkbenchFileFields, type FileFieldView } from "./workbench-file-fields";
import { FileFieldPane } from "./file-field-pane";

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

// Reserved rows move out of the grid only when the profile names them.
// Their text and styles stay under profile control, including non-key footers.
function isFooterRow(row: ScreenRow): boolean {
  return row.role === "reserved" && !!row.footer;
}

function footerContent(row: ScreenRow): preact.ComponentChildren {
  if (row.footer_text === undefined) return styledText(row.t, row.s, false);
  // The physical frame splits reserved text at the prefix. A named footer
  // carries its complete display text separately, before that split.
  if (row.t && row.s?.length && row.footer_text.endsWith(row.t))
    return <>{row.footer_text.slice(0, -row.t.length)}{styledText(row.t, row.s, false)}</>;
  return row.footer_text;
}

function statusContent(line: string, message: Snapshot["status_pmsg"]): preact.ComponentChildren {
  if (!message || !Number.isInteger(message.start) || !Number.isInteger(message.length)) return line;
  const points = Array.from(line);
  const start = message.start, end = start + message.length;
  if (start < 0 || message.length <= 0 || end > points.length) return line;
  const severity = ["ERROR", "WARNING", "INFORMATION"].includes(message.severity) ? message.severity.toLowerCase() : "information";
  return <>{points.slice(0, start).join("")}<span class={`status-pmsg status-pmsg-${severity}`} style={panelColour(message.colour)} title={message.text}>{points.slice(start, end).join("")}</span>{points.slice(end).join("")}</>;
}

function styledText(text: string, styles: StyleRun[] | undefined, showTabs = true) {
  if (!styles?.length) return showTabs ? text.replaceAll("\t", "→") : text;

  const characters = Array.from(text, (character) => showTabs && character === "\t" ? "→" : character);
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

type NamedLogView = {
  generation: string;
  contentRevision: string;
  nextCursor: string;
  nextOffset: number;
  records: WorkbenchLogRecord[];
  omitted: boolean;
  omittedBytes: string;
  omittedRecords: string;
  more: boolean;
};

type DocumentPosition = { line: number; offset: number };
type NamedDocumentView = {
  page: WorkbenchDocumentPage;
  start: DocumentPosition;
  previous: DocumentPosition[];
};
type DocumentRequest = DocumentPosition & { revision: string; previous: DocumentPosition[] };

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
  const [latestMessage, setLatestMessage] = useState("");
  const [storageState, setStorageState] = useState("");
  const [toolSession,setToolSession]=useState<ToolSession|null>(null);
  const [command, setCommand] = useState("");
  const [commandFocused, setCommandFocused] = useState(false);
  const [actions, setActions] = useState<FrontendActionDefinition[]>([]);
  const [workbench, setWorkbench] = useState<WorkbenchCatalog | null>(null);
  const [panelState, setPanelState] = useState<WorkbenchPanelStateMessage | null>(null);
  const [logViews, setLogViews] = useState<Record<string, NamedLogView>>({});
  const [documentViews, setDocumentViews] = useState<Record<string, NamedDocumentView>>({});
  const [documentErrors, setDocumentErrors] = useState<Record<string, string>>({});
  const [documentLoading, setDocumentLoading] = useState<Record<string, boolean>>({});
  const [formViews, setFormViews] = useState<Record<string, FormView>>({});
  const [fileFieldViews, setFileFieldViews] = useState<Record<string, FileFieldView>>({});
  const [selectedAux, setSelectedAux] = useState("console");
  const [showProgramWithLog, setShowProgramWithLog] = useState(false);
  const [openMenu, setOpenMenu] = useState<"application" | "about" | null>(null);
  const [consoleOpen, setConsoleOpen] = useState(false);
  const [toolsOpen, setToolsOpen] = useState(false);
  const [programFocusRequest, setProgramFocusRequest] = useState(0);
  const [toolArguments, setToolArguments] = useState("");
  const [mobilePane, setMobilePane] = useState<"files" | "console">("files");
  const [filesWidth, setFilesWidth] = useState(216);
  const [auxWidth, setAuxWidth] = useState(400);
  const [windowSize, setWindowSize] = useState({ width: window.innerWidth, height: window.innerHeight });
  const [commandHistoryOpen, setCommandHistoryOpen] = useState(false);
  const [dockEntries, setDockEntries] = useState<DockEntry[]>([]);
  const [pathAction, setPathAction] = useState<FrontendActionDefinition | WorkbenchActionDefinition | null>(null);
  const [explorerAction, setExplorerAction] = useState<WorkbenchActionDefinition | null>(null);
  const [pathValue, setPathValue] = useState("");
  const [explorerOpen, setExplorerOpen] = useState(false);
  const [files, setFiles] = useState<FileListMessage | null>(null);
  const socketRef = useRef<EditorTransport | null>(null);
  const inputRef = useRef<HTMLTextAreaElement | null>(null);
  const commandInputRef = useRef<HTMLInputElement | null>(null);
  const pathInputRef = useRef<HTMLInputElement | null>(null);
  const nextId = useRef(1);
  const viewportRef = useRef<HTMLDivElement | null>(null);
  const gridRef = useRef<HTMLDivElement | null>(null);
  const cellProbeRef = useRef<HTMLSpanElement | null>(null);
  const snapshotRef = useRef<Snapshot | null>(null);
  const appliedCoreCommandRef = useRef("");
  const highlightPresentationRef = useRef(new HighlightPresentation());
  const lastFileFocusRef = useRef<Snapshot["focus"] | null>(null);
  const viewportSizeRef = useRef("");
  const messageSerialRef = useRef<number | null>(null);
  const dockIdRef = useRef(0);
  const menuRef = useRef<HTMLDivElement | null>(null);
  const menuButtonRef = useRef<HTMLButtonElement | null>(null);
  const consoleButtonRef = useRef<HTMLButtonElement | null>(null);
  const mobileConsoleButtonRef = useRef<HTMLButtonElement | null>(null);
  const menuReturnRef = useRef<HTMLElement | null>(null);
  const pathReturnRef = useRef<HTMLElement | null>(null);
  const pathFocusReturnRef = useRef<HTMLElement | null>(null);
  const pathFocusReturnPendingRef = useRef(false);
  const workbenchPagesRef = useRef(new WorkbenchPageCollector());
  const panelStateRef = useRef<WorkbenchPanelStateMessage | null>(null);
  const logViewsRef = useRef<Record<string, NamedLogView>>({});
  const logPendingRef = useRef(new Set<string>());
  const documentViewsRef = useRef<Record<string, NamedDocumentView>>({});
  const documentPendingRef = useRef(new Map<string, DocumentRequest>());
  const formsRef = useRef<WorkbenchForms | null>(null);
  const fileFieldsRef = useRef<WorkbenchFileFields | null>(null);
  const pendingWorkbenchRef = useRef(new Set<string>());
  const formFocusRef = useRef<HTMLDivElement | null>(null);
  const formViewportsRef = useRef(new Map<string, FormViewport>());
  const focusSerialRef = useRef("0");
  const panelFocusOriginRef = useRef<HTMLElement | null>(null);
  const auxRailRef = useRef<HTMLDivElement | null>(null);
  const namedLogRef = useRef<HTMLPreElement | null>(null);
  const logScrollRef = useRef(new Map<string, { top: number; left: number; follow: boolean }>());
  const layoutReportsRef = useRef(new Map<string, string>());
  const workbenchSessionRef = useRef("");
  const workbenchNextRequestRef = useRef(1);
  const responseLogRef = useRef<HTMLDivElement | null>(null);
  const followingResponsesRef = useRef(true);
  const lastShownJob = useRef(0);
  const lastShownBuild = useRef(0);

  const appendDockEntry = useCallback((kind: DockEntry["kind"], text: string) => {
    if (!text.trim()) return;
    const entry = { id: ++dockIdRef.current, kind, text };
    setDockEntries((entries) => [...entries.slice(-199), entry]);
  }, []);

  const send = useCallback((message: Record<string, unknown>) => {
    const socket = socketRef.current;
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    const id = nextId.current++;
    if (typeof message.requestId === "string") pendingWorkbenchRef.current.add(message.requestId);
    socket.send(JSON.stringify({ v: 1, id, ...message }));
    return id;
  }, []);

  const requestLogPage = useCallback((model: string, generation: string,
    view?: NamedLogView) => {
    const key = `${model}:${generation}`;
    if (logPendingRef.current.has(key)) return;
    const current = view ?? logViewsRef.current[key];
    if (send({ type: "workbench", version: 1, operation: "log.list",
      model, generation, cursor: current?.nextCursor ?? "0",
      offset: current?.nextOffset ?? 0, records: 8, bytes: 512 }) !== undefined)
      logPendingRef.current.add(key);
  }, [send]);

  const requestDocumentPage = useCallback((panel: WorkbenchPanelDefinition | WorkbenchPanelState,
    start?: DocumentPosition, previous?: DocumentPosition[]) => {
    if (!panel.documentAvailable || !panel.documentRevision) return;
    const key = `${panel.id}:${panel.generation}`;
    if (documentPendingRef.current.has(key)) return;
    const view = documentViewsRef.current[key];
    const unchanged = view?.page.documentRevision === panel.documentRevision;
    const position = start ?? { line: Math.max(1, Math.min(view?.start.line ?? 1, panel.lineCount ?? 1)),
      offset: unchanged ? view?.start.offset ?? 0 : 0 };
    if (send({ type: "workbench", version: 1, operation: "document.list", panel: panel.id,
      expectGeneration: panel.generation, expectRevision: panel.documentRevision,
      ...position, records: 8, bytes: 8192 }) !== undefined) {
      documentPendingRef.current.set(key, { ...position, revision: panel.documentRevision,
        previous: previous ?? (unchanged ? view?.previous ?? [] : []) });
      setDocumentLoading((loading) => ({ ...loading, [key]: true }));
      setDocumentErrors((errors) => ({ ...errors, [key]: "" }));
    }
  }, [send]);

  const focusEditor = useCallback(() => inputRef.current?.focus(), []);
  const rememberPathFocus = useCallback(() => {
    const active = document.activeElement;
    pathReturnRef.current = menuRef.current?.contains(active)
      ? menuReturnRef.current
      : active instanceof HTMLElement ? active : null;
  }, []);
  const closePathDialog = useCallback(() => {
    pathFocusReturnRef.current = pathReturnRef.current;
    pathFocusReturnPendingRef.current = true;
    pathReturnRef.current = null;
    setPathAction(null);
  }, []);

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
        appliedCoreCommandRef.current = "";
        setConnection("online");
        workbenchPagesRef.current.reset();
        workbenchSessionRef.current = "";
        panelStateRef.current = null;
        logViewsRef.current = {};
        documentViewsRef.current = {};
        documentPendingRef.current.clear();
        pendingWorkbenchRef.current.clear();
        formsRef.current?.reset();
        fileFieldsRef.current?.reset();
        formViewportsRef.current.clear();
        logScrollRef.current.clear();
        logPendingRef.current.clear();
        layoutReportsRef.current.clear();
        focusSerialRef.current = "0";
        panelFocusOriginRef.current = null;
        setWorkbench(null);
        setPanelState(null);
        setLogViews({});
        setDocumentViews({});
        setDocumentErrors({});
        setDocumentLoading({});
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
          if ("type" in message && message.type === "hello") {
            const hello = message as DriverMessage;
            workbenchSessionRef.current = hello.workbenchSession ?? "";
            workbenchNextRequestRef.current = hello.workbenchNextRequest ?? 1;
          } else if ("type" in message && message.type === "tools") {
            setToolSession(message as unknown as ToolSession);

          } else if ("type" in message && message.type === "storage") {
            const storage = message as DriverMessage & { state: string };
            setStorageState(storage.state === "error" ? storage.message ?? "Storage error" : storage.state === "saving" ? "Saving browser workspace…" : "Saved to browser workspace");
          } else if ("type" in message && message.type === "fatal") {
            setConnection("offline");
            setLastError(message.message ?? "Browser editor could not start");
          } else if ("mode" in message && "screen_rows" in message) {
            const nextSnapshot = message as Snapshot;
            if (nextSnapshot.last_message?.trim()) setLatestMessage(nextSnapshot.last_message);
            if (typeof nextSnapshot.last_message_serial === "number") {
              if (messageSerialRef.current !== null && messageSerialRef.current !== nextSnapshot.last_message_serial)
                appendDockEntry("response", nextSnapshot.last_message ?? "");
              messageSerialRef.current = nextSnapshot.last_message_serial;
            }
            if (snapshotRef.current?.buffer?.path !== nextSnapshot.buffer?.path) lastFileFocusRef.current = null;
            if (nextSnapshot.focus.zone === "filearea") lastFileFocusRef.current = nextSnapshot.focus;
            snapshotRef.current = nextSnapshot;
            setSnapshot(nextSnapshot);
            if (nextSnapshot.focus.zone === "command"
                && appliedCoreCommandRef.current !== nextSnapshot.command) {
              appliedCoreCommandRef.current = nextSnapshot.command;
              setCommand(nextSnapshot.command);
            }
            setLastError("");
          } else if ("type" in message && message.type === "actions" && message.actions) {
            setActions(message.actions);
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "list") {
            const listing = message as unknown as { data: WorkbenchPage; publication?: number };
            const catalog = workbenchPagesRef.current.accept(listing.data, listing.publication);
            if (catalog) setWorkbench(catalog);
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "panel-state") {
            const next = message as WorkbenchPanelStateMessage;
            panelStateRef.current = next;
            setPanelState(next);
            formsRef.current?.observe(next.panels);
            fileFieldsRef.current?.observe(next.panels);
            const liveKeys = new Set(next.panels.map((panel) => `${panel.id}:${panel.generation}`));
            const expired = Object.keys(documentViewsRef.current).filter((key) => !liveKeys.has(key));
            if (expired.length) {
              const retained = { ...documentViewsRef.current };
              expired.forEach((key) => delete retained[key]);
              documentViewsRef.current = retained;
              setDocumentViews(retained);
            }
            for (const key of documentPendingRef.current.keys())
              if (!liveKeys.has(key)) documentPendingRef.current.delete(key);
            for (const key of logScrollRef.current.keys())
              if (!liveKeys.has(key)) logScrollRef.current.delete(key);
            for (const key of formViewportsRef.current.keys())
              if (!liveKeys.has(key)) formViewportsRef.current.delete(key);
            setDocumentLoading((loading) => Object.fromEntries(Object.entries(loading).filter(([key]) => liveKeys.has(key))));
            setDocumentErrors((errors) => Object.fromEntries(Object.entries(errors).filter(([key]) => liveKeys.has(key))));
            for (const panel of next.panels) {
              if (!panel.visible) continue;
              if (panel.kind === "fields" || panel.kind === "file-fields") continue;
              if (panel.kind === "document") {
                const view = documentViewsRef.current[`${panel.id}:${panel.generation}`];
                if (!view || view.page.documentRevision !== panel.documentRevision)
                  requestDocumentPage(panel);
                continue;
              }
              const key = `${panel.model}:${panel.modelGeneration}`;
              const view = logViewsRef.current[key];
              if (!view || view.contentRevision !== panel.contentRevision || view.more)
                requestLogPage(panel.model, panel.modelGeneration);
            }
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "log") {
            const page = (message as unknown as { data: WorkbenchLogPage }).data;
            const key = `${page.id}:${page.generation}`;
            logPendingRef.current.delete(key);
            const live = panelStateRef.current?.panels.find((panel) =>
              panel.model === page.id && panel.modelGeneration === page.generation);
            if (!live || !Array.isArray(page.records)) return;
            const prior = logViewsRef.current[key];
            const reset = !prior || page.omitted || prior.generation !== page.generation;
            const records = reset ? [...page.records] : [...prior.records, ...page.records];
            let characters = records.reduce((total, record) => total + record.text.length, 0);
            let trimmed = false;
            while (characters > 262144 && records.length > 1) {
              characters -= records.shift()!.text.length;
              trimmed = true;
            }
            const next: NamedLogView = {
              generation: page.generation, contentRevision: page.contentRevision,
              nextCursor: page.nextCursor, nextOffset: page.nextOffset, records,
              omitted: !!page.omitted || !!prior?.omitted || trimmed,
              omittedBytes: page.omittedBytes, omittedRecords: page.omittedRecords,
              more: page.more,
            };
            logViewsRef.current = { ...logViewsRef.current, [key]: next };
            setLogViews(logViewsRef.current);
            if (page.more || page.contentRevision !== live.contentRevision)
              requestLogPage(page.id, page.generation, next);
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "document") {
            const page = (message as unknown as { data: WorkbenchDocumentPage }).data;
            const key = `${page.panel}:${page.panelGeneration}`;
            const pending = documentPendingRef.current.get(key);
            if (!pending || pending.revision !== page.documentRevision) return;
            documentPendingRef.current.delete(key);
            setDocumentLoading((loading) => ({ ...loading, [key]: false }));
            const live = panelStateRef.current?.panels.find((panel) => panel.id === page.panel
              && panel.generation === page.panelGeneration);
            if (!live || !live.documentAvailable || live.documentId !== page.documentId) return;
            if (live.documentRevision !== page.documentRevision) {
              requestDocumentPage(live);
              return;
            }
            if (!Array.isArray(page.records) || page.records.length > 8) return;
            const next = { page, start: { line: pending.line, offset: pending.offset }, previous: pending.previous };
            documentViewsRef.current = { ...documentViewsRef.current, [key]: next };
            setDocumentViews(documentViewsRef.current);
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "form") {
            formsRef.current?.page((message as unknown as { data: WorkbenchFormPage }).data);
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "filefield") {
            fileFieldsRef.current?.page((message as unknown as { data: WorkbenchFileFieldPage }).data);
          } else if ("type" in message && message.type === "workbench" && (message as { operation?: string }).operation === "result") {
            const result = message as { code?: string; message?: string; requestId?: string; state?: string; draftRevision?: string };
            if (result.state !== "queued" && result.requestId) pendingWorkbenchRef.current.delete(result.requestId);
            formsRef.current?.result(result);
            fileFieldsRef.current?.result(result);
            if (!result.requestId) {
              logPendingRef.current.clear();
              if (result.code && result.code !== "ok") {
                const errors = Object.fromEntries([...documentPendingRef.current.keys()]
                  .map((key) => [key, result.message || result.code!]));
                setDocumentErrors((prior) => ({ ...prior, ...errors }));
                setDocumentLoading((loading) => ({ ...loading,
                  ...Object.fromEntries([...documentPendingRef.current.keys()].map((key) => [key, false])) }));
              }
              documentPendingRef.current.clear();
            }
            const prefix = `${workbenchSessionRef.current}.`;
            if (result.requestId?.startsWith(prefix)) {
              const sequence = Number(result.requestId.slice(prefix.length));
              if (Number.isSafeInteger(sequence) && sequence > 0)
                workbenchNextRequestRef.current = Math.max(workbenchNextRequestRef.current, sequence + 1);
            }
            if (result.state !== "queued" && result.code && result.code !== "ok") {
              const reason = result.message || result.code;
              setLastError(reason);
              appendDockEntry("error", reason);
            }
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
  }, [appendDockEntry, focusEditor, requestDocumentPage, requestLogPage]);

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

  useLayoutEffect(() => {
    if (!pathAction) return;
    pathInputRef.current?.focus();
    const focus = window.setTimeout(() => pathInputRef.current?.focus(), 0);
    return () => window.clearTimeout(focus);
  }, [pathAction]);

  useLayoutEffect(() => {
    if (pathAction || !pathFocusReturnPendingRef.current) return;
    pathFocusReturnPendingRef.current = false;
    const prior = pathFocusReturnRef.current;
    pathFocusReturnRef.current = null;
    if (prior?.isConnected && !prior.matches(":disabled") && prior.getClientRects().length)
      prior.focus();
    else focusEditor();
  }, [pathAction, focusEditor]);

  useEffect(() => {
    if (toolSession?.job.id && toolSession.job.id !== lastShownJob.current) {
      lastShownJob.current = toolSession.job.id;
      setConsoleOpen(true);
      setSelectedAux("console");
      setMobilePane("console");
    }
  }, [toolSession?.job.id]);

  useEffect(() => {
    const id = toolSession?.build?.job.id;
    if (id && id !== lastShownBuild.current) {
      lastShownBuild.current = id;
      setToolsOpen(true);
      setSelectedAux("tools");
      setShowProgramWithLog(false);
      setMobilePane("console");
    }
  }, [toolSession?.build?.job.id]);

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
    (action: string, argument = "", restoreFocus = true) => {
      send({ type: "action", action, argument });
      setOpenMenu(null);
      if (restoreFocus) window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  const rememberPanelFocusOrigin = useCallback(() => {
    if (panelFocusOriginRef.current?.isConnected) return;
    const active = document.activeElement;
    if (active instanceof HTMLElement && active.closest(".named-panel-pane")) return;
    const origin = menuRef.current?.contains(active) ? menuReturnRef.current : active;
    if (origin instanceof HTMLElement) panelFocusOriginRef.current = origin;
  }, []);

  const sendPanelOperation = useCallback((panel: string, generation: string,
    operation: "show" | "hide" | "focus" | "return") => {
    if (!workbenchSessionRef.current) return;
    if (operation === "focus") rememberPanelFocusOrigin();
    const requestId = `${workbenchSessionRef.current}.${workbenchNextRequestRef.current++}`;
    if (send({ type: "workbench", version: 1, operation: `panel.${operation}`,
      requestId, panel, expectGeneration: generation }) !== undefined) return requestId;
  }, [rememberPanelFocusOrigin, send]);

  if (!formsRef.current) formsRef.current = new WorkbenchForms({
    send, changed: setFormViews,
    nextId: () => `${workbenchSessionRef.current}.${workbenchNextRequestRef.current++}`,
    available: () => !!workbenchSessionRef.current && !pendingWorkbenchRef.current.size,
    focus: (panel) => sendPanelOperation(panel.id, panel.generation, "focus"),
    returning: (panel) => sendPanelOperation(panel.id, panel.generation, "return"),
  });

  if (!fileFieldsRef.current) fileFieldsRef.current = new WorkbenchFileFields({
    send, changed: setFileFieldViews,
    nextId: () => `${workbenchSessionRef.current}.${workbenchNextRequestRef.current++}`,
    available: () => !!workbenchSessionRef.current && !pendingWorkbenchRef.current.size,
    focus: (panel) => sendPanelOperation(panel.id, panel.generation, "focus"),
    returning: (panel) => sendPanelOperation(panel.id, panel.generation, "return"),
  });

  const selectProgramConsole = useCallback(() => {
    const focused = panelStateRef.current?.panels.find((panel) => panel.focused);
    if (focused) sendPanelOperation(focused.id, focused.generation, "return");
    setConsoleOpen(true);
    setSelectedAux("console");
    setShowProgramWithLog(false);
    setMobilePane("console");
  }, [sendPanelOperation]);

  const selectToolsProfiles = useCallback(() => {
    const focused = panelStateRef.current?.panels.find((panel) => panel.focused);
    if (focused) sendPanelOperation(focused.id, focused.generation, "return");
    setToolsOpen(true);
    setSelectedAux("tools");
    setShowProgramWithLog(false);
    setMobilePane("console");
  }, [sendPanelOperation]);

  const sendWorkbenchAction = useCallback((action: WorkbenchActionDefinition, argument?: string,
    restoreFocus = true) => {
    if (!workbenchSessionRef.current) {
      setLastError("Workbench session is not ready");
      return;
    }
    if (action.kind === "panel" && action.operation === "focus")
      rememberPanelFocusOrigin();
    send({ type: "workbench", version: 1, operation: "invoke",
      requestId: `${workbenchSessionRef.current}.${workbenchNextRequestRef.current++}`, action: action.id,
      expectGeneration: action.generation, ...(argument === undefined ? {} : { argument }) });
    setOpenMenu(null);
    if (action.kind === "panel" && action.operation === "show" && action.panel) {
      const origin = menuReturnRef.current;
      setShowProgramWithLog(!!origin?.closest(".program-console-pane") && consoleOpen);
      setSelectedAux(action.panel);
      setMobilePane("console");
    }
    if (restoreFocus && action.kind === "panel" && action.operation !== "focus") {
      const origin = menuReturnRef.current;
      window.setTimeout(() => { if (origin?.isConnected) origin.focus(); }, 0);
    } else if (restoreFocus && action.kind !== "panel")
      window.setTimeout(focusEditor, 0);
  }, [consoleOpen, focusEditor, rememberPanelFocusOrigin, send]);

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
        setExplorerAction(null);
        setExplorerOpen(true);
        setMobilePane("files");
        requestFiles();
        return;
      }
      if (action.requires_argument && argument === undefined) {
        rememberPathFocus();
        setPathValue("");
        setPathAction(action);
        return;
      }
      sendAction(id, argument ?? "");
    },
    [actionById, rememberPathFocus, requestFiles, sendAction],
  );

  const requestWorkbenchAction = useCallback((action: WorkbenchActionDefinition, argument?: string) => {
    if (action.reason !== "ok" && action.reason !== "argument-required") return;
    if (action.kind === "builtin" && action.builtin === "file.open"
        && !action.operand && argument === undefined) {
      setExplorerAction(action);
      setExplorerOpen(true);
      setMobilePane("files");
      requestFiles();
      setOpenMenu(null);
      return;
    }
    if ((action.reason === "argument-required"
        || (action.kind === "builtin" && !action.operand
            && ["file.create", "buffer.switch"].includes(action.builtin)))
        && argument === undefined) {
      rememberPathFocus();
      setPathAction(action);
      setPathValue("");
      setOpenMenu(null);
      return;
    }
    sendWorkbenchAction(action, argument);
  }, [rememberPathFocus, requestFiles, sendWorkbenchAction]);

  const footerRows = useMemo(() => snapshot?.panel ? [] :
    (snapshot?.screen_rows ?? []).filter(isFooterRow), [snapshot?.screen_rows, !!snapshot?.panel]);
  const footerRowIds = new Set(footerRows.map((row) => row.r));

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
        // Named rows are shown in a wrapping footer, so their model rows
        // take no space in this grid. Fill the recovered rows with file text.
        const namedRows = current.screen_rows.filter(isFooterRow).length;
        const rows = Math.max(12, Math.min(256, Math.floor((viewport.clientHeight - padding) / lineHeight) + overhead + namedRows));
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
  }, [connection, send, snapshot?.buffer?.path, !!snapshot?.panel, footerRows.length]);

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
    if (!openMenu && (event.target as HTMLElement).closest(".named-panel-pane")
        && (event.key === "Escape" || event.key === "F6")) {
      event.preventDefault(); event.stopPropagation();
      const pane = (event.target as HTMLElement).closest<HTMLElement>(".named-panel-pane");
      const state = panelStateRef.current?.panels.find((item) => item.id === pane?.dataset.panelId);
      if (state?.kind === "fields" && state.focused) formsRef.current?.intent(state, "return");
      else if (state?.kind === "file-fields" && state.focused) fileFieldsRef.current?.intent(state, "return");
      else if (state?.focused) sendPanelOperation(state.id, state.generation, "return");
      else returnToFile();
      return;
    }
    if (!openMenu && (event.target as HTMLElement).closest(".named-document-pane")
      && ((!event.ctrlKey && !event.metaKey && event.key.length === 1)
        || ["Backspace", "Delete", "Insert"].includes(event.key)
        || event.key === "Enter" && !(event.target instanceof HTMLButtonElement)
        || (event.ctrlKey || event.metaKey) && ["s", "z", "y", "x", "v"].includes(event.key.toLowerCase()))) {
      event.preventDefault(); event.stopPropagation();
      return;
    }
    if (!openMenu && (event.target as HTMLElement).closest(".named-fields-pane, .named-file-fields-pane")
      && (event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
      event.preventDefault(); event.stopPropagation(); return;
    }
    if (!openMenu && event.key === "Tab" && event.shiftKey && !event.ctrlKey && !event.altKey && !event.metaKey &&
        (event.target === consoleButtonRef.current || event.target === mobileConsoleButtonRef.current)) {
      event.preventDefault();
      menuButtonRef.current?.focus();
      return;
    }
    if (!openMenu && (event.target as HTMLElement).closest(".program-console-pane, .console-toggle, .tools-profiles-pane, .tools-toggle, .current-file-tools") &&
        (event.key === "F6" || event.key === "Escape")) {
      event.preventDefault();
      event.stopPropagation();
      if (event.key === "F6") commandInputRef.current?.focus();
      else returnToFile();
      return;
    }
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
  }, [openMenu, pathAction, returnToFile, sendPanelOperation, snapshot?.panel]);

  const onEditorKeyDown = useCallback(
    (event: KeyboardEvent) => {
      if (snapshot?.panel && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        return;
      }
      const commandModifier = event.ctrlKey || event.metaKey;
      if (!snapshot?.panel && event.key === "Tab" && event.shiftKey && !commandModifier && !event.altKey) {
        event.preventDefault();
        const consoleButton = [consoleButtonRef.current, mobileConsoleButtonRef.current]
          .find((button) => button?.getClientRects().length);
        (consoleButton ?? menuButtonRef.current)?.focus();
        return;
      }
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

  const rows = useMemo(() => highlightPresentationRef.current.rows(snapshot), [snapshot]);
  const currentBuffer = snapshot?.buffers.find((buffer) => buffer.current);
  const title = currentBuffer ? basename(currentBuffer.path) : "CRXTHE";
  const cursorRow = useMemo(
    () => rows.find((row) => row.r === snapshot?.focus.row && row.line === snapshot?.focus.line),
    [rows, snapshot?.focus.line, snapshot?.focus.row],
  );
  const prefixCols = snapshot?.viewport?.prefix_cols ?? 7;
  const prefixGap = snapshot?.viewport?.prefix_gap ?? 1;
  const menus = useMemo(() => {
    if (workbench) {
      const actionById = new Map(workbench.actions.map((action) => [action.id, action]));
      return [...workbench.menus]
        .sort((a, b) => a.order - b.order)
        .map((menu) => ({ id: menu.id, label: menu.label, entries: [...workbench.items]
          .filter((item) => item.menu === menu.id)
          .sort((a, b) => a.order - b.order)
          .map((item) => ({ item, action: actionById.get(item.action) })) }));
    }
    const result = new Map<string, FrontendActionDefinition[]>();
    for (const action of actions) {
      if (!action.menu) continue;
      const entries = result.get(action.menu) ?? [];
      entries.push(action);
      result.set(action.menu, entries);
    }
    return [...result.entries()].map(([label, entries]) => ({ id: label, label,
      entries: entries.map((action) => ({ item: { id: action.id, label: action.label, separator: false }, action })) }));
  }, [actions, workbench]);

  const hitRow = useCallback(
    (event: MouseEvent, row: ScreenRow, target: "filearea" | "prefix") => {
      const cell = pointerCell(event, target === "filearea" ? row.sc : 0);
      send({ type: "hit", target, line: row.line, row: row.r, cell });
      window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  const currentFileAction = (action: FileToolAction) => {
    if (fileActionReason(toolSession, snapshot, connection === "online", action)) return;
    const file = toolSession?.currentFile;
    if (!file || send({ type: "tool", action: `file-${action.id}`, tool: action.tool, text: "", arguments: toolArguments,
      sourceId: file.sourceId, revision: file.revision,
      ...(action.generation ? { generation: action.generation } : {}) }) === undefined) return;
    if (action.id === "run") {
      selectProgramConsole();
      setProgramFocusRequest((request) => request + 1);
    }
  };
  const runWithTool = (tool: string, image: string) => {
    const file = toolSession?.currentFile;
    if (send({ type: "tool", action: tool === "rxbin" ? "rxbin" : "run", tool, text: image,
      arguments: toolArguments, sourceId: file?.sourceId, revision: file?.revision }) === undefined) return;
    selectProgramConsole();
    setProgramFocusRequest((request) => request + 1);
  };
  const maximumFilesWidth = Math.max(180, Math.min(400, Math.floor(windowSize.width * .25)));
  const maximumAuxWidth = Math.max(300, Math.min(650, Math.floor(windowSize.width * .36)));
  const visibleNamedPanels = useMemo(() => {
    const states = new Map(panelState?.panels.map((panel) => [panel.id, panel]));
    return (workbench?.panels ?? []).flatMap((panel) => {
      const state = states.get(panel.id);
      if (state && state.generation !== panel.generation) return [];
      const combined = { ...panel, ...state };
      return combined.visible ? [combined] : [];
    });
  }, [panelState, workbench]);
  const showAux = consoleOpen || toolsOpen || visibleNamedPanels.length > 0;
  const activeAux = selectedAux === "console" && consoleOpen ? "console"
    : selectedAux === "tools" && toolsOpen ? "tools"
    : visibleNamedPanels.some((panel) => panel.id === selectedAux) ? selectedAux
    : visibleNamedPanels[0]?.id ?? (consoleOpen ? "console" : "tools");
  const activeNamedPanel = visibleNamedPanels.find((panel) => panel.id === activeAux);
  const placementFor = (panel: WorkbenchPanelDefinition) => {
    const wide = windowSize.width >= 1100;
    const phone = windowSize.width <= 640;
    for (const candidate of [panel.placement, ...panel.fallbacks]) {
      if (candidate === "right" && !wide || candidate === "below" && phone) continue;
      const width = candidate === "below" || !wide ? windowSize.width
        : Math.min(auxWidth, maximumAuxWidth);
      const height = candidate === "right" ? windowSize.height - 130 : 410;
      if (Math.floor(width / 8) >= panel.mincols && Math.floor(height / 18) >= panel.minrows)
        return candidate;
    }
    return "none";
  };
  const activePanelPlacement = activeNamedPanel ? placementFor(activeNamedPanel) : "right";
  const activeNamedLog = activeNamedPanel
    && activeNamedPanel.kind === "log"
    ? logViews[`${activeNamedPanel.model}:${activeNamedPanel.modelGeneration}`] : undefined;
  const activePanelKey = activeNamedPanel ? `${activeNamedPanel.id}:${activeNamedPanel.generation}` : "";
  if ((activeNamedPanel?.kind === "fields" || activeNamedPanel?.kind === "file-fields") && !formViewportsRef.current.has(activePanelKey))
    formViewportsRef.current.set(activePanelKey, { top: 0, left: 0, field: "", start: 0, end: 0 });
  const documentView = documentViews[activePanelKey];
  const activeDocumentPage = activeNamedPanel?.documentAvailable
    && documentView?.page.documentId === activeNamedPanel.documentId
    && documentView.page.documentRevision === activeNamedPanel.documentRevision ? documentView.page : undefined;
  const activeMobilePane = mobilePane === "files" && explorerOpen ? "files"
    : mobilePane === "console" && showAux ? "console"
    : explorerOpen ? "files" : "console";
  const latestResponse = [...dockEntries].reverse().find((entry) => entry.kind !== "command");

  useEffect(() => {
    const focus = panelState?.focus;
    if (!focus || focus.serial === focusSerialRef.current) return;
    focusSerialRef.current = focus.serial;
    if (focus.serial === "0") return;
    if (focus.restoring) {
      const origin = panelFocusOriginRef.current;
      panelFocusOriginRef.current = null;
      if (origin?.closest(".program-console-pane")) {
        setConsoleOpen(true);
        setSelectedAux("console");
        setMobilePane("console");
      }
      window.requestAnimationFrame(() => window.requestAnimationFrame(() => {
        if (origin?.isConnected && origin.getClientRects().length) origin.focus();
        else focusEditor();
      }));
    } else if (focus.panel) {
      if (!panelFocusOriginRef.current) {
        const active = document.activeElement;
        if (active instanceof HTMLElement && !active.closest(".named-panel-pane"))
          panelFocusOriginRef.current = active;
      }
      setSelectedAux(focus.panel);
      setShowProgramWithLog(false);
      setMobilePane("console");
      window.requestAnimationFrame(() => window.requestAnimationFrame(() => {
        if (formFocusRef.current) formFocusRef.current.focus();
        else namedLogRef.current?.focus();
      }));
    }
  }, [focusEditor, panelState?.focus]);

  useLayoutEffect(() => {
    if (!showAux || connection !== "online") return;
    const rectangle = (consoleOpen && showProgramWithLog && activeNamedPanel
      ? auxRailRef.current?.querySelector(".named-panel-pane") : auxRailRef.current)?.getBoundingClientRect();
    if (!rectangle) return;
    const cols = Math.max(0, Math.floor(rectangle.width / 8));
    const rows = Math.max(0, Math.floor(rectangle.height / 18));
    for (const panel of visibleNamedPanels) {
      const desired = panel.id === activeAux ? placementFor(panel) : "inactive-tab";
      const placement = desired === "inactive-tab" || (cols >= panel.mincols && rows >= panel.minrows)
        ? desired : "none";
      const report = `${panel.generation}:${placement}:${cols}:${rows}`;
      if (layoutReportsRef.current.get(panel.id) === report) continue;
      if (send({ type: "workbench", version: 1, operation: "panel.layout",
        panel: panel.id, expectGeneration: panel.generation,
        placement, cols, rows }) !== undefined)
        layoutReportsRef.current.set(panel.id, report);
    }
    for (const id of [...layoutReportsRef.current.keys()])
      if (!visibleNamedPanels.some((panel) => panel.id === id)) layoutReportsRef.current.delete(id);
  }, [activeAux, auxWidth, connection, consoleOpen, send, showAux, showProgramWithLog, visibleNamedPanels, windowSize]);

  useEffect(() => {
    if (!activeNamedPanel || activeNamedPanel.kind !== "log" || !namedLogRef.current) return;
    const key = `${activeNamedPanel.id}:${activeNamedPanel.generation}`;
    if (logScrollRef.current.get(key)?.follow === false) return;
    namedLogRef.current.scrollTop = namedLogRef.current.scrollHeight;
  }, [activeNamedPanel?.id, activeNamedLog?.records.length]);
  const toggleFiles = () => {
    if (window.innerWidth >= 1100 || activeMobilePane === "files") setExplorerOpen(!explorerOpen);
    else setExplorerOpen(true);
    setMobilePane("files");
    if (!explorerOpen) requestFiles();
  };
  const showOrFocusPanel = (panel: WorkbenchPanelDefinition) => {
    const state = panelState?.panels.find((item) => item.id === panel.id
      && item.generation === panel.generation);
    const visible = state?.visible ?? panel.visible;
    setSelectedAux(panel.id);
    setShowProgramWithLog(false);
    setMobilePane("console");
    sendPanelOperation(panel.id, panel.generation, visible ? "focus" : "show");
  };

  return (
    <main class="app-shell" onKeyDownCapture={onWorkbenchKeyDown} style={{...consoleColourVariables(snapshot?.console_colours), ...diagnosticColourVariables(snapshot?.diagnostic_colours)}}>
      <header class="toolbar">
        <div class="product-mark">CRXTHE</div>
        <div class="menu-root" ref={menuRef}>
          <button type="button" ref={menuButtonRef} class={`menu-trigger ${openMenu ? "menu-trigger-open" : ""}`}
            aria-expanded={!!openMenu} aria-controls="application-menu"
            onPointerDown={() => {
              const active = document.activeElement;
              menuReturnRef.current = active instanceof HTMLElement ? active : menuButtonRef.current;
            }}
            onClick={(event) => {
              if (event.detail === 0) menuReturnRef.current = menuButtonRef.current;
              setOpenMenu(openMenu ? null : "application");
            }}><Menu size={16} /> Menu</button>
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
            {menus.map((menu) => <section key={menu.id} aria-label={menu.label} data-menu-id={menu.id}>
              <h2>{menu.label}</h2>
              {menu.entries.map(({ item, action }) => item.separator ? <hr key={item.id} /> : action ? <button
                key={item.id} type="button" data-item-id={item.id}
                data-action-id={action.id} data-generation={"generation" in action ? action.generation : undefined}
                disabled={"generation" in action && action.reason !== "ok" && action.reason !== "argument-required"}
                title={"generation" in action && action.reason !== "ok" && action.reason !== "argument-required" ? action.reason : undefined}
                onClick={() => "generation" in action ? requestWorkbenchAction(action) : requestAction(action.id)}>
                <span>{item.label}</span>
                <kbd>{("generation" in action ? action.builtin : action.id) === "file.create" ? "Ctrl+N" : ("generation" in action ? action.builtin : action.id) === "file.open" ? "Ctrl+O" : ("generation" in action ? action.builtin : action.id) === "file.save" ? "Ctrl+S" : ("generation" in action ? action.builtin : action.id) === "file.close" ? "Ctrl+W" : ("generation" in action ? action.builtin : action.id) === "edit.undo" ? "Ctrl+Z" : ""}</kbd>
                {"generation" in action && action.reason !== "ok" && action.reason !== "argument-required"
                  ? <small class="menu-action-reason">{action.reason.replaceAll("-", " ")}</small> : null}
              </button> : null)}
            </section>)}
            <section aria-label="Workbench"><h2>Workbench</h2>
              <button type="button" onClick={() => { toggleFiles(); setOpenMenu(null); }}>Files</button>
              <button type="button" onClick={() => { setConsoleOpen(true); setSelectedAux("console"); setMobilePane("console"); setOpenMenu(null); }}>Program console</button>
              <button type="button" onClick={() => { selectToolsProfiles(); setOpenMenu(null); }}>Tools & Profiles</button>
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
        <div class="document-title" title={currentBuffer?.path}>
          <span class="document-name">{title}</span>
          {browserWasm || (snapshot?.parser && snapshot.parser.state !== "inactive") ? <div class="workspace-indicators" aria-label="Workspace and syntax status">
            {browserWasm ? <span class="workspace-storage" role="status">{storageState}</span> : null}
            {snapshot?.parser && snapshot.parser.state !== "inactive" ? <button class="parser-status" title="Restart the highlighter" onClick={() => send({type: "parser-restart"})}>Syntax: {snapshot.parser.state}</button> : null}
          </div> : null}
        </div>
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
      <div class="workbench-header">
      <div class="pane-switcher" role="group" aria-label="Workbench panes">
        <button type="button" aria-pressed={explorerOpen} onClick={toggleFiles}>Files</button>
        <button type="button" class="console-toggle" ref={consoleButtonRef} aria-pressed={activeAux === "console" && consoleOpen} onClick={() => {
          if (panelStateRef.current?.panels.some((panel) => panel.focused)) selectProgramConsole();
          else { setConsoleOpen(window.innerWidth < 1100 && activeMobilePane !== "console" ? true : !consoleOpen); setSelectedAux("console"); setMobilePane("console"); }
        }}>
          Console{toolSession?.job.waiting ? " · Input requested" : toolSession?.job.running ? " · Running" : ""}
        </button>
        <button type="button" class="tools-toggle" aria-pressed={activeAux === "tools" && toolsOpen}
          onClick={selectToolsProfiles}>Tools & Profiles</button>
        {workbench?.panels.map((panel) => <button type="button" key={panel.id}
          class={`named-${panel.kind === "fields" ? "form" : panel.kind}-toggle`} data-panel-id={panel.id}
          onPointerDown={rememberPanelFocusOrigin}
          aria-pressed={activeAux === panel.id && visibleNamedPanels.some((item) => item.id === panel.id)}
          onClick={() => showOrFocusPanel(panel)}>{panel.title}</button>)}
      </div>
      <CurrentFileActions session={toolSession} snapshot={snapshot} online={connection === "online"}
        onAction={currentFileAction} onSetup={selectToolsProfiles} />
      </div>
      <section class={`workspace mobile-pane-${activeMobilePane} ${explorerOpen ? "files-open" : ""} ${showAux ? "aux-open" : ""} ${activeNamedPanel && activePanelPlacement === "below" ? "aux-below" : ""}`} style={{
        "--files-width": `${Math.min(filesWidth, maximumFilesWidth)}px`, "--aux-width": `${Math.min(auxWidth, maximumAuxWidth)}px`,
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
                      if (explorerAction) {
                        requestWorkbenchAction(explorerAction, entry.target);
                        setExplorerAction(null);
                      } else requestAction("file.open", entry.target);
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
                <div class={`screen-row role-${row.role} ${row.cur ? "screen-row-current" : ""} ${footerRowIds.has(row.r) ? "profile-footer-source" : ""}`} key={`${currentBuffer?.path ?? ""}-${row.r}-${row.line}-${row.role}`}>
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
            {typeof snapshot?.status_line === "string" ? <pre class="profile-status">{statusContent(snapshot.status_line, snapshot.status_pmsg)}</pre> : <>
            <span>Ln {snapshot?.focus.line ?? 0}, Col {(snapshot?.focus.cell ?? 0) + 1}</span>
            <span>{snapshot?.focus.zone === "prefix" ? "Prefix" : snapshot?.focus.zone === "command" || commandFocused ? "Command" : "File"}</span>
            <span>{snapshot?.buffer?.dirty ? "Modified" : "Saved"}</span>
            </>}
          </div>
          {footerRows.filter((row) => row.footer !== "response").map((row) => <pre class={`profile-footer-row ${row.footer === "keys" ? "key-hints" : ""}`} data-footer={row.footer} key={`${row.footer}-${row.r}`} style={panelColour(row.footer_colour)}>{footerContent(row)}</pre>)}
        </section>

        <footer class="command-area" aria-label="THE commands and responses">
          <div id="command-responses" class="command-responses" hidden={!commandHistoryOpen} ref={responseLogRef}
            aria-label="THE command and response history" tabIndex={0}
            onScroll={(event) => { const log = event.currentTarget; followingResponsesRef.current = log.scrollHeight - log.scrollTop - log.clientHeight < 24; }}>
            {dockEntries.length ? dockEntries.map((entry) => <div class={`dock-entry dock-entry-${entry.kind}`} key={entry.id}>
              <span>{entry.kind === "command" ? "Command" : entry.kind === "error" ? "Error" : "Response"}</span><code>{entry.text}</code>
            </div>) : <p>Commands and editor messages appear here.</p>}
          </div>
          <div class="latest-response" role="status" aria-live="polite" hidden={!lastError && !latestResponse && !latestMessage}>{lastError || latestResponse?.text || latestMessage}</div>
          {!snapshot?.panel ? <form class="command-form" onSubmit={(event) => { event.preventDefault(); runCommand(command); }}>
            <label class="command-prompt" for="the-command">THE</label>
            <input id="the-command" aria-label="THE command" ref={commandInputRef}
              onFocus={() => setCommandFocused(true)} onBlur={() => setCommandFocused(false)}
              onKeyDown={(event) => {
                if (event.key === "Escape") { event.preventDefault(); focusEditor(); }
                if (event.key === "F6") { event.preventDefault(); returnToFile(); }
              }} value={command} onInput={(event) => setCommand(event.currentTarget.value)} spellcheck={false} autoComplete="off" />
            <button type="submit" disabled={connection !== "online" || !command.trim()}>Execute</button>
            <button type="button" class="command-history-toggle" aria-expanded={commandHistoryOpen} aria-controls="command-responses" onClick={() => setCommandHistoryOpen(!commandHistoryOpen)}>
              {commandHistoryOpen ? "Hide history" : "History"}{dockEntries.length ? ` (${dockEntries.length})` : ""}
            </button>
          </form> : null}
        </footer>
        </div>

        <div class="mobile-aux-tabs" role="group" aria-label="Choose auxiliary pane">
          <button type="button" aria-pressed={activeMobilePane === "files" && explorerOpen} onClick={() => { setExplorerOpen(true); setMobilePane("files"); requestFiles(); }}>Files</button>
          <button type="button" class="console-toggle" ref={mobileConsoleButtonRef} aria-pressed={activeMobilePane === "console" && activeAux === "console" && consoleOpen} onClick={selectProgramConsole}>Console{toolSession?.job.waiting ? " · Input requested" : ""}</button>
          <button type="button" class="tools-toggle" aria-pressed={activeMobilePane === "console" && activeAux === "tools" && toolsOpen}
            onClick={selectToolsProfiles}>Tools & Profiles</button>
          {workbench?.panels.map((panel) => <button type="button" key={panel.id}
            data-panel-id={panel.id} aria-pressed={activeMobilePane === "console" && activeAux === panel.id}
            onPointerDown={rememberPanelFocusOrigin} onClick={() => showOrFocusPanel(panel)}>{panel.title}</button>)}
        </div>
        {showAux ? <PaneSplitter label="Resize auxiliary panes" orientation="vertical" reverse consoleDivider={activeAux === "console" && consoleOpen} value={Math.min(auxWidth, maximumAuxWidth)} minimum={300} maximum={maximumAuxWidth} onChange={setAuxWidth} /> : null}
        <div class={`auxiliary-rail ${consoleOpen && showProgramWithLog && activeNamedPanel ? "program-and-log" : ""}`} ref={auxRailRef} hidden={!showAux}>
        <div class="auxiliary-tabs" role="tablist" aria-label="Auxiliary panes">
          {consoleOpen ? <button type="button" role="tab" aria-selected={activeAux === "console"}
            onClick={selectProgramConsole}>Program console</button> : null}
          {toolsOpen ? <button type="button" role="tab" aria-selected={activeAux === "tools"}
            onClick={selectToolsProfiles}>Tools & Profiles</button> : null}
          {visibleNamedPanels.map((panel) => <button type="button" key={panel.id}
            role="tab" data-panel-id={panel.id} aria-selected={activeAux === panel.id}
            onPointerDown={rememberPanelFocusOrigin} onClick={() => showOrFocusPanel(panel)}>{panel.title}</button>)}
        </div>
        <div class="program-console-pane" hidden={!consoleOpen || activeAux !== "console" && !(showProgramWithLog && activeNamedPanel)}>
          {toolSession ? <RexxConsole send={send} session={toolSession} snapshot={snapshot} focusRequest={programFocusRequest} onClose={() => { setConsoleOpen(false); setShowProgramWithLog(false); focusEditor(); }} focusEditor={focusEditor} />
            : <p class="empty-directory">Program console unavailable: no program tools were provided.</p>}
        </div>
        <div class="tools-profiles-pane" hidden={!toolsOpen || activeAux !== "tools"}>
          {toolSession ? <ToolsProfiles send={send} session={toolSession} snapshot={snapshot} online={connection === "online"}
            argumentsText={toolArguments} onArguments={setToolArguments} onProgramRun={runWithTool}
            onClose={() => { setToolsOpen(false); focusEditor(); }} focusEditor={focusEditor} />
            : <p class="empty-directory">Tools unavailable: waiting for the editor.</p>}
        </div>
        {activeNamedPanel ? <div class={`named-panel-pane named-${activeNamedPanel.kind}-pane`} data-panel-id={activeNamedPanel.id}
          onPointerDown={rememberPanelFocusOrigin}
          onBeforeInput={(event) => { if (activeNamedPanel.kind === "document") { event.preventDefault(); event.stopPropagation(); } }}
          onPaste={(event) => { if (activeNamedPanel.kind === "document") { event.preventDefault(); event.stopPropagation(); } }}
          onDrop={(event) => { if (activeNamedPanel.kind === "document") { event.preventDefault(); event.stopPropagation(); } }}
          aria-label={activeNamedPanel.title} role="tabpanel">
          <div class="named-panel-heading"><strong>{activeNamedPanel.title}</strong>
            <button type="button" onClick={() => sendPanelOperation(activeNamedPanel.id, activeNamedPanel.generation, "hide")}>Hide</button>
          </div>
          {activeNamedPanel.kind === "log" && activeNamedPanel.modelError && activeNamedPanel.modelError !== "ok"
            ? <p class="named-log-error" role="status">Log route error: {activeNamedPanel.modelError}</p> : null}
          {activeNamedPanel.kind === "log" && (activeNamedLog?.omitted || !!activeNamedPanel.omittedBytes && activeNamedPanel.omittedBytes !== "0")
            ? <p class="named-log-omitted" role="status">Older output omitted</p> : null}
          {activeNamedPanel.effectivePlacement === "none" && activeNamedPanel.reason !== "hidden"
            ? <p class="named-log-unavailable" role="status">{activeNamedPanel.reason.replaceAll("-", " ")}</p> : null}
          {activeNamedPanel.kind === "document" ? <>
            <p class="named-document-status" role="status">Read only · {activeNamedPanel.lineCount ?? 0} lines
              {!activeNamedPanel.documentAvailable ? ` · ${(activeNamedPanel.documentReason || "unavailable document").replaceAll("-", " ")}`
                : documentErrors[activePanelKey] ? ` · ${documentErrors[activePanelKey]}`
                : !activeDocumentPage ? " · Loading document…" : ""}</p>
            <div class="named-document-navigation" role="group" aria-label="Document pages">
              <button type="button" disabled={!activeDocumentPage || !documentView.previous.length || documentLoading[activePanelKey]}
                onClick={() => {
                  const previous = [...documentView.previous];
                  const position = previous.pop();
                  if (position) requestDocumentPage(activeNamedPanel, position, previous);
                }}>Previous</button>
              <button type="button" disabled={!activeDocumentPage?.more || documentLoading[activePanelKey]}
                onClick={() => {
                  if (activeDocumentPage) requestDocumentPage(activeNamedPanel,
                    { line: activeDocumentPage.nextLine, offset: activeDocumentPage.nextOffset },
                    [...documentView.previous, documentView.start].slice(-256));
                }}>Next</button>
              <button type="button" disabled={!activeNamedPanel.documentAvailable || documentLoading[activePanelKey]}
                onClick={() => requestDocumentPage(activeNamedPanel)}>Refresh</button>
            </div>
          </> : null}
          {activeNamedPanel.kind === "fields" ? activePanelPlacement !== "none" ? <NamedFormPane panel={activeNamedPanel}
            view={formViews[`${activeNamedPanel.form}:${activeNamedPanel.formGeneration}`]} forms={formsRef.current!}
            viewport={formViewportsRef.current.get(activePanelKey)!}
            focusRef={(node) => { formFocusRef.current = node; }} /> : null :
          activeNamedPanel.kind === "file-fields" ? activePanelPlacement !== "none" ? <FileFieldPane panel={activeNamedPanel}
            view={fileFieldViews[activePanelKey]} fields={fileFieldsRef.current!}
            viewport={formViewportsRef.current.get(activePanelKey)!}
            focusRef={(node) => { formFocusRef.current = node; }} /> : null :
          <pre class={`named-panel-output named-${activeNamedPanel.kind}-output`} hidden={activePanelPlacement === "none"} ref={(node) => {
            const key = `${activeNamedPanel.id}:${activeNamedPanel.generation}`;
            if (node) {
              namedLogRef.current = node;
              const saved = logScrollRef.current.get(key);
              node.scrollTop = saved?.top ?? (activeNamedPanel.kind === "log" ? node.scrollHeight : 0);
              node.scrollLeft = saved?.left ?? 0;
            } else if (namedLogRef.current?.dataset.panelKey === key) namedLogRef.current = null;
          }} tabIndex={0}
            data-panel-key={`${activeNamedPanel.id}:${activeNamedPanel.generation}`}
            aria-label={`${activeNamedPanel.title} ${activeNamedPanel.kind === "document" ? "read only document" : "output"}`}
            onScroll={(event) => {
              const node = event.currentTarget;
              logScrollRef.current.set(`${activeNamedPanel.id}:${activeNamedPanel.generation}`, {
                top: node.scrollTop, left: node.scrollLeft,
                follow: node.scrollHeight - node.scrollTop - node.clientHeight < 24,
              });
            }}>{activeNamedPanel.kind === "document" ? activeDocumentPage?.records.map((record) =>
              <span class="named-document-line" key={`${record.line}:${record.offset}`}><span class="named-document-line-number" aria-hidden="true">{record.line}{record.offset ? "+" : ""}</span><span>{displayLogText(record.text)}</span></span>)
              : activeNamedLog?.records.map((record) => <span key={`${record.cursor}:${record.offset}`}
              class={record.stream === 2 ? "named-log-stderr" : "named-log-stdout"}>
              {displayLogText(record.text)}</span>)}</pre>}
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
          if (event.currentTarget === event.target) closePathDialog();
        }}>
          <form class="path-dialog" role="dialog" aria-modal="true" aria-labelledby="path-dialog-title"
          onKeyDown={(event) => {
            if (event.key === "Escape") {
              event.preventDefault(); event.stopPropagation(); closePathDialog();
            }
          }} onSubmit={(event) => {
            event.preventDefault();
            if (!pathValue) return;
            const value = pathValue;
            if ("generation" in pathAction) sendWorkbenchAction(pathAction, value, false);
            else sendAction(pathAction.id, value, false);
            closePathDialog();
          }}>
            <div class="dialog-heading">
              <h2 id="path-dialog-title">{pathAction.label} file</h2>
              <button class="icon-button" type="button" aria-label="Close" title="Close" onClick={closePathDialog}><X size={17} /></button>
            </div>
            <label for="file-path">Path</label>
            <input id="file-path" ref={pathInputRef} autoFocus value={pathValue} onInput={(event) => setPathValue(event.currentTarget.value)} spellcheck={false} autoComplete="off" />
            <div class="dialog-actions">
              <button type="button" onClick={closePathDialog}>Cancel</button>
              <button type="submit" class="primary-command" disabled={!pathValue}>{pathAction.label}</button>
            </div>
          </form>
        </div>
      ) : null}
    </main>
  );
}

render(<App />, document.getElementById("app")!);
