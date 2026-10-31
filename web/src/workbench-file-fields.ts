import type { WorkbenchFileFieldPage, WorkbenchFileFieldRow, WorkbenchPanelDefinition, WorkbenchPanelState } from "./types";

type Panel = WorkbenchPanelDefinition | WorkbenchPanelState;
type Operation = "apply" | "cancel" | "reload" | "return";
export type FileFieldView = {
  revision: string; writeRevision: string; dirty: boolean; active: boolean;
  rows: WorkbenchFileFieldRow[]; values: Record<string, string>;
  error: string; notice: string; loading: boolean; busy: boolean; locked: boolean;
  carets: Record<string, { offset: number; serial: number }>;
};
type FieldEdit = { field: string; edit: "insert" | "backspace" | "delete" | "clear"; offset: number; codepoint?: number; panel: Panel; base: string; follow: boolean };
type Draft = FileFieldView & {
  edits: FieldEdit[];
  readRevision: string;
  queued: Map<string, { value: string; composing: boolean; base: string; panel: Panel }>;
  intent?: { operation: Operation; panel: Panel };
};
type Flight = { id: string; key: string; operation: string; base: string; field?: string; value?: string; revision?: string; edit?: FieldEdit; offset?: number };
const token = (value: unknown): value is string => typeof value === "string" && /^(0|[1-9][0-9]*)$/.test(value);

// This holds only unadmitted widget text and presentation copies. The core owns
// the projection draft, the write token, validation and the complete Apply.
export class WorkbenchFileFields {
  private drafts = new Map<string, Draft>();
  private panels = new Map<string, Panel>();
  private reads = new Map<string, { revision: string; rows: WorkbenchFileFieldRow[] }>();
  private flight: Flight | null = null;
  private focusFlight: { id: string; key: string } | null = null;
  constructor(private io: {
    send: (message: Record<string, unknown>) => unknown;
    nextId: () => string; available: () => boolean;
    focus: (panel: Panel) => string | undefined; returning: (panel: Panel) => void;
    changed: (views: Record<string, FileFieldView>) => void;
  }) {}
  reset() { this.drafts.clear(); this.panels.clear(); this.reads.clear(); this.flight = null; this.focusFlight = null; this.publish(); }
  private key(panel: Panel) { return `${panel.id}:${panel.generation}`; }
  private publish() {
    this.io.changed(Object.fromEntries([...this.drafts].map(([key, draft]) => [key, { ...draft,
      rows: [...draft.rows], values: { ...draft.values }, carets: { ...draft.carets }, busy: !!draft.intent || this.flight?.key === key,
      locked: !!draft.intent || this.flight?.key === key && !["set", "edit"].includes(this.flight.operation) }])));
  }
  observe(panels: Panel[]) {
    this.panels = new Map(panels.map((panel) => [panel.id, panel]));
    const live = new Set(panels.filter((panel) => panel.kind === "file-fields").map((panel) => this.key(panel)));
    for (const key of this.drafts.keys()) if (!live.has(key)) {
      const draft = this.drafts.get(key)!;
      // Keep unadmitted text inspectable if a pane disappears; never transfer it
      // to a replacement carrying the same public ID and a new generation.
      if (Object.keys(draft.values).length) this.fail(draft, "stale-generation: File-field pane is unavailable; draft retained");
      else this.drafts.delete(key);
      this.reads.delete(key);
    }
    if (this.focusFlight) {
      const panel = panels.find((item) => this.key(item) === this.focusFlight!.key);
      if (!panel || panel.focused) this.focusFlight = null;
    }
    for (const panel of panels) {
      if (panel.kind !== "file-fields" || !token(panel.draftRevision) || !token(panel.writeRevision)) continue;
      const key = this.key(panel);
      let draft = this.drafts.get(key);
      if (!draft) {
        draft = { revision: panel.draftRevision, writeRevision: panel.writeRevision,
          dirty: !!panel.projectionDirty, active: panel.projectionActive !== false,
          rows: [], values: {}, carets: {}, edits: [], error: "", notice: "", loading: false, busy: false, locked: false,
          readRevision: "", queued: new Map() };
        this.drafts.set(key, draft);
      }
      if (draft.revision !== panel.draftRevision && this.flight?.key !== key
        && (Object.keys(draft.values).length || draft.edits.length) && !draft.error)
        this.fail(draft, "stale-revision: Fields changed while editing; draft retained. Reload or Cancel to reconcile.");
      draft.revision = panel.draftRevision; draft.writeRevision = panel.writeRevision;
      draft.dirty = !!panel.projectionDirty; draft.active = panel.projectionActive !== false;
      if (panel.visible && draft.readRevision !== draft.revision) this.read(panel);
    }
    this.pump(); this.publish();
  }
  private read(panel: Panel) {
    const key = this.key(panel), draft = this.drafts.get(key);
    if (!draft || this.reads.has(key)) return;
    this.reads.set(key, { revision: draft.revision, rows: [] }); draft.loading = true;
    this.io.send({ type: "workbench", version: 1, operation: "filefield.list", panel: panel.id,
      expectGeneration: panel.generation, expectRevision: draft.revision, offset: 0, records: 8, bytes: 8192 });
  }
  page(page: WorkbenchFileFieldPage) {
    const key = `${page.panel}:${page.panelGeneration}`, draft = this.drafts.get(key), read = this.reads.get(key);
    const panel = this.panels.get(page.panel);
    if (!draft || !read || !panel || this.key(panel) !== key) return;
    if (page.schemaVersion !== 1 || page.section !== "file-fields" || !token(page.draftRevision)
      || !token(page.writeRevision) || page.documentId !== panel.documentId
      || !Number.isSafeInteger(page.total) || page.total < 0 || page.total > 128
      || !Array.isArray(page.records) || page.records.length > 8 || page.offset !== read.rows.length
      || page.offset + page.records.length > page.total || page.records.some((row) =>
        !Number.isSafeInteger(row.line) || row.line < 1 || !Number.isSafeInteger(row.cells) || row.cells < 0
        || !Array.isArray(row.spans) || row.spans.length > 128 || row.spans.some((span) =>
          typeof span.value !== "string" || !Number.isSafeInteger(span.cell) || span.cell < 0
          || !Number.isSafeInteger(span.maxcells) || span.maxcells < 0 || span.cell + span.maxcells > row.cells))) {
      this.reads.delete(key); draft.loading = false; this.fail(draft, "Invalid file-field page; draft retained"); this.publish(); return;
    }
    if (page.draftRevision !== read.revision || page.draftRevision !== draft.revision) {
      this.reads.delete(key); draft.loading = false; this.read(panel); return;
    }
    read.rows.push(...page.records);
    if (page.nextOffset !== null) {
      if (page.nextOffset !== read.rows.length || page.nextOffset >= page.total || !page.records.length) {
        this.reads.delete(key); draft.loading = false; this.fail(draft, "Invalid file-field continuation; draft retained"); this.publish(); return;
      }
      this.io.send({ type: "workbench", version: 1, operation: "filefield.list", panel: panel.id,
        expectGeneration: panel.generation, expectRevision: draft.revision,
        offset: page.nextOffset, records: 8, bytes: 8192 }); return;
    }
    if (read.rows.length !== page.total) return;
    draft.rows = read.rows; draft.readRevision = page.draftRevision; draft.writeRevision = page.writeRevision;
    draft.dirty = page.dirty; draft.active = page.active; draft.loading = false;
    this.reads.delete(key); this.finish(); this.pump(); this.publish();
  }
  private live(panel: Panel) {
    const live = this.panels.get(panel.id);
    return live && live.visible && live.kind === "file-fields" && this.key(live) === this.key(panel) ? live : undefined;
  }
  private fail(draft: Draft, message: string) { draft.error = message; draft.queued.clear(); draft.intent = undefined; }
  focus(panel: Panel) {
    const draft = this.drafts.get(this.key(panel));
    if (!draft || this.live(panel)?.focused || this.focusFlight || !this.io.available()) return;
    if (!this.live(panel)) this.fail(draft, "stale-generation: File-field pane is unavailable; draft retained");
    else { const id = this.io.focus(panel); if (id) this.focusFlight = { id, key: this.key(panel) }; }
    this.publish();
  }
  change(panel: Panel, field: string, value: string, composing: boolean) {
    const key = this.key(panel), draft = this.drafts.get(key);
    if (!draft || draft.error || !draft.active || draft.intent && !draft.queued.get(field)?.composing
      || !draft.rows.some((row) => row.spans.some((span) => span.id === field && span.editable))) return;
    if ((draft.intent?.operation === "cancel" || draft.intent?.operation === "reload") && !composing) {
      draft.queued.delete(field); delete draft.values[field]; this.pump(); this.publish(); return;
    }
    const base = draft.queued.get(field)?.base ?? (this.flight?.key === key ? this.flight.base : draft.revision);
    draft.values[field] = value; draft.queued.set(field, { value, composing, base, panel });
    this.pump(); this.publish();
  }
  edit(panel: Panel, field: string, edit: FieldEdit["edit"], offset: number, codepoint?: number) {
    const key = this.key(panel), draft = this.drafts.get(key);
    if (!draft || draft.error || draft.intent || !draft.active || !Number.isSafeInteger(offset) || offset < 0
      || !draft.rows.some((row) => row.spans.some((span) => span.id === field && span.editable))) return;
    const previous = draft.edits.at(-1) ?? (this.flight?.key === key ? this.flight.edit : undefined);
    if (previous && (previous.field !== field || previous.offset !== offset)) {
      draft.notice = "Wait for the pending field edit before moving its caret."; this.publish(); return;
    }
    draft.notice = "";
    const base = this.flight?.key === key ? this.flight.base : draft.revision;
    draft.edits.push({ field, edit, offset, codepoint, panel, base, follow: !!previous });
    this.pump(); this.publish();
  }
  selectionError(panel: Panel) {
    const draft = this.drafts.get(this.key(panel));
    if (draft) { draft.notice = "Partial selection editing is unavailable. Use a caret or select the complete field."; this.publish(); }
  }
  intent(panel: Panel, operation: Operation) {
    const draft = this.drafts.get(this.key(panel)); if (!draft || draft.intent) return;
    if (operation === "cancel" || operation === "reload") {
      draft.error = ""; draft.notice = ""; draft.edits = [];
      for (const [field, queued] of draft.queued) if (!queued.composing) draft.queued.delete(field);
      // Discard local text only after an admitted core Cancel/Reload receipt.
    } else if (draft.error && operation !== "return") return;
    draft.intent = { operation, panel }; this.pump(); this.publish();
  }
  result(result: { requestId?: string; state?: string; code?: string; message?: string; draftRevision?: string; utf16Caret?: number }) {
    if (result.state === "queued") return;
    if (!result.requestId && result.code && result.code !== "ok") {
      for (const key of this.reads.keys()) {
        const draft = this.drafts.get(key); if (draft) { draft.loading = false; this.fail(draft, result.message || result.code); }
      }
      this.reads.clear(); this.publish(); return;
    }
    if (this.focusFlight && this.focusFlight.id === result.requestId) {
      if (result.code !== "ok") {
        const draft = this.drafts.get(this.focusFlight.key);
        if (draft) this.fail(draft, result.message || result.code || "File-field focus refused; draft retained");
        this.focusFlight = null;
      }
      this.pump(); this.publish(); return;
    }
    if (!this.flight || this.flight.id !== result.requestId) { this.pump(); this.publish(); return; }
    const draft = this.drafts.get(this.flight.key);
    if (!draft) this.flight = null;
    else if (result.code !== "ok") {
      this.fail(draft, `${result.code || "Request failed"}: ${result.message || "File-field request refused"}. Draft retained.`);
      this.flight = null;
    } else if (!token(result.draftRevision)) {
      this.fail(draft, "Missing file-field revision receipt; draft retained"); this.flight = null;
    } else {
      this.flight.revision = result.draftRevision;
      this.flight.offset = result.utf16Caret;
      // Read directly from the receipt too: result/state ordering is transport independent.
      draft.revision = result.draftRevision;
      const panel = this.panels.get(this.flight.key.slice(0, this.flight.key.lastIndexOf(":")));
      if (panel && draft.readRevision !== draft.revision) this.read(panel);
      this.finish();
    }
    this.pump(); this.publish();
  }
  private finish() {
    if (!this.flight?.revision) return;
    const draft = this.drafts.get(this.flight.key);
    if (!draft || draft.readRevision !== this.flight.revision) return;
    if (["cancel", "reload"].includes(this.flight.operation)) draft.values = {};
    else if (this.flight.field && draft.values[this.flight.field] === this.flight.value) delete draft.values[this.flight.field];
    for (const queued of draft.queued.values()) if (queued.base === this.flight.base) queued.base = this.flight.revision;
    for (const edit of draft.edits) if (edit.base === this.flight.base) edit.base = this.flight.revision;
    if (this.flight.operation === "edit" && this.flight.field && Number.isSafeInteger(this.flight.offset)) {
      const field = this.flight.field, offset = this.flight.offset!;
      draft.carets[field] = { offset, serial: (draft.carets[field]?.serial ?? 0) + 1 };
      const next = draft.edits[0]; if (next?.field === field && next.follow) next.offset = offset;
    }
    this.flight = null;
  }
  private pump() {
    if (this.flight || this.focusFlight || !this.io.available()) return;
    for (const [key, draft] of this.drafts) {
      if (draft.loading || draft.readRevision !== draft.revision) continue;
      const queued = !draft.error ? [...draft.queued].find(([, item]) => !item.composing) : undefined;
      if (queued && queued[1].base !== draft.revision) { this.fail(draft, "stale-revision: Fields changed while editing; draft retained"); continue; }
      const edit = !queued && !draft.queued.size && !draft.error ? draft.edits[0] : undefined;
      if (edit && edit.base !== draft.revision) { this.fail(draft, "stale-revision: Fields changed while editing; draft retained"); continue; }
      const intent = !queued && !draft.queued.size && !edit ? draft.intent : undefined;
      if (!queued && !edit && !intent) continue;
      const panel = queued ? queued[1].panel : edit ? edit.panel : intent!.panel;
      const live = this.live(panel);
      if (!live) { this.fail(draft, "stale-generation: File-field pane is unavailable; draft retained"); continue; }
      if (!live.focused) { this.focus(panel); return; }
      if (intent?.operation === "return") { draft.intent = undefined; this.io.returning(panel); return; }
      const operation = queued ? "set" : edit ? "edit" : intent!.operation, id = this.io.nextId();
      this.flight = { id, key, operation, base: draft.revision, field: queued?.[0] ?? edit?.field, value: queued?.[1].value, edit };
      if (queued) draft.queued.delete(queued[0]); else if (edit) draft.edits.shift(); else draft.intent = undefined;
      this.io.send({ type: "workbench", version: 1, operation: `filefield.${operation}`, requestId: id,
        panel: panel.id, expectGeneration: panel.generation, expectRevision: draft.revision,
        ...(queued ? { field: queued[0], value: queued[1].value } : edit ? { field: edit.field, edit: edit.edit, offset: edit.offset, ...(edit.codepoint === undefined ? {} : { codepoint: edit.codepoint }) } : {}) });
      return;
    }
  }
}
