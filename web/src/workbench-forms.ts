import type { WorkbenchFormField, WorkbenchFormPage, WorkbenchPanelDefinition, WorkbenchPanelState } from "./types";

type Panel = WorkbenchPanelDefinition | WorkbenchPanelState;
export type FormView = {
  form: string; generation: string; revision: string; resultRevision: string;
  dirty: boolean; submitted: boolean; fields: WorkbenchFormField[];
  values: Record<string, string>; error: string; loading: boolean; busy: boolean; locked: boolean;
};
type WorkingForm = FormView & {
  fieldsRevision: string; queued: Map<string, { value: string; panel: Panel; composing: boolean; base: string }>;
  intent?: { operation: "submit" | "cancel" | "return"; panel: Panel };
};
type Read = { revision: string; fields: WorkbenchFormField[] };
type Flight = { id: string; key: string; field?: string; value?: string; operation: string; base: string; revision?: string };

export class WorkbenchForms {
  private forms = new Map<string, WorkingForm>();
  private reads = new Map<string, Read>();
  private panels = new Map<string, Panel>();
  private flight: Flight | null = null;
  private focusFlight: { id: string; key: string; panel: Panel } | null = null;
  constructor(private io: {
    send: (message: Record<string, unknown>) => unknown;
    nextId: () => string;
    available: () => boolean;
    focus: (panel: Panel) => string | undefined;
    returning: (panel: Panel) => void;
    changed: (views: Record<string, FormView>) => void;
  }) {}
  reset() { this.forms.clear(); this.reads.clear(); this.panels.clear(); this.flight = null; this.focusFlight = null; this.publish(); }
  private key(panel: Panel) { return `${panel.form}:${panel.formGeneration}`; }
  private publish() {
    this.io.changed(Object.fromEntries([...this.forms].map(([key, form]) => [key, { ...form,
      fields: [...form.fields], values: { ...form.values }, busy: !!form.intent || this.flight?.key === key,
      locked: !!form.intent || this.flight?.key === key && this.flight.operation !== "set" }])));
  }
  observe(panels: Panel[]) {
    this.panels = new Map(panels.map((panel) => [panel.id, panel]));
    if (this.focusFlight) {
      const live = this.livePanel(this.focusFlight.panel);
      if (!live || live.focused) this.focusFlight = null;
    }
    const live = new Set(panels.filter((panel) => panel.kind === "fields").map((panel) => this.key(panel)));
    for (const key of this.forms.keys()) if (!live.has(key)) { this.forms.delete(key); this.reads.delete(key); }
    for (const panel of panels) {
      if (panel.kind !== "fields" || !panel.form || !panel.formGeneration || !panel.draftRevision) continue;
      const key = this.key(panel);
      let form = this.forms.get(key);
      if (!form) {
        form = { form: panel.form, generation: panel.formGeneration, revision: panel.draftRevision,
          resultRevision: panel.resultRevision ?? "0", dirty: !!panel.formDirty, submitted: !!panel.submitted,
          fields: [], values: {}, error: "", loading: false, busy: false, locked: false, fieldsRevision: "", queued: new Map() };
        this.forms.set(key, form);
      }
      if (form.revision !== panel.draftRevision && this.flight?.key !== key
        && (form.queued.size || Object.keys(form.values).length) && !form.error) this.stale(form);
      form.revision = panel.draftRevision; form.resultRevision = panel.resultRevision ?? "0";
      form.dirty = !!panel.formDirty; form.submitted = !!panel.submitted;
      if (panel.visible && form.fieldsRevision !== form.revision) this.read(key);
    }
    this.pump(); this.publish();
  }
  private read(key: string) {
    const form = this.forms.get(key);
    if (!form || this.reads.has(key)) return;
    this.reads.set(key, { revision: form.revision, fields: [] }); form.loading = true;
    this.io.send({ type: "workbench", version: 1, operation: "form.list", form: form.form,
      offset: 0, records: 8, bytes: 8192 });
  }
  page(page: WorkbenchFormPage) {
    const key = `${page.form}:${page.generation}`, form = this.forms.get(key), read = this.reads.get(key);
    if (!form || !read || !Array.isArray(page.records) || page.records.length > 8 || page.total > 128) return;
    if (page.draftRevision !== read.revision || page.draftRevision !== form.revision || page.offset !== read.fields.length) {
      this.reads.delete(key); form.loading = false; this.read(key); return;
    }
    read.fields.push(...page.records);
    if (page.nextOffset !== null) {
      if (page.nextOffset !== read.fields.length || page.nextOffset >= page.total) return;
      this.io.send({ type: "workbench", version: 1, operation: "form.list", form: form.form,
        offset: page.nextOffset, records: 8, bytes: 8192 }); return;
    }
    if (read.fields.length !== page.total) return;
    form.fields = read.fields; form.fieldsRevision = page.draftRevision; form.loading = false;
    this.reads.delete(key); this.finish(); this.pump(); this.publish();
  }
  private livePanel(panel: Panel) {
    const live = this.panels.get(panel.id);
    return live && live.visible && live.generation === panel.generation && live.kind === "fields"
      && live.form === panel.form && live.formGeneration === panel.formGeneration ? live : undefined;
  }
  private requestFocus(panel: Panel) {
    const form = this.forms.get(this.key(panel));
    if (!form || form.error || this.focusFlight || !this.io.available()) return;
    if (!this.livePanel(panel)) { this.unavailable(form); return; }
    const id = this.io.focus(panel);
    if (id) this.focusFlight = { id, key: this.key(panel), panel };
  }
  focus(panel: Panel) { if (!this.livePanel(panel)?.focused) { this.requestFocus(panel); this.publish(); } }
  change(panel: Panel, field: string, value: string, composing: boolean) {
    const key = this.key(panel), form = this.forms.get(key);
    if (!form || form.intent && !form.queued.get(field)?.composing || form.error
      || !form.fields.some((item) => item.id === field && item.access === "edit")) return;
    if (form.intent?.operation === "cancel" && !composing) {
      form.queued.delete(field); delete form.values[field]; this.pump(); this.publish(); return;
    }
    const base = form.queued.get(field)?.base ?? (this.flight?.key === key ? this.flight.base : form.revision);
    form.values[field] = value; form.queued.set(field, { value, panel, composing, base });
    this.pump(); this.publish();
  }
  intent(panel: Panel, operation: "submit" | "cancel" | "return") {
    const form = this.forms.get(this.key(panel));
    if (!form || form.intent) return;
    if (operation === "cancel") {
      form.error = "";
      for (const [field, value] of form.queued) if (!value.composing) form.queued.delete(field);
      for (const field of Object.keys(form.values)) if (!form.queued.has(field)) delete form.values[field];
    }
    if (form.error && operation !== "return") return;
    form.intent = { operation, panel }; this.pump(); this.publish();
  }
  reload(panel: Panel) {
    const form = this.forms.get(this.key(panel)); if (!form || this.flight?.key === this.key(panel)) return;
    form.error = ""; form.values = {}; form.queued.clear(); form.fieldsRevision = "";
    this.read(this.key(panel)); this.publish();
  }
  result(result: { requestId?: string; state?: string; code?: string; message?: string; draftRevision?: string }) {
    if (result.state === "queued") return;
    if (this.focusFlight && this.focusFlight.id === result.requestId) {
      if (result.code !== "ok") {
        const form = this.forms.get(this.focusFlight.key);
        if (form) {
          form.error = result.message || result.code || "Form focus refused";
          form.queued.clear(); form.intent = undefined;
        }
        this.focusFlight = null;
      }
      this.pump(); this.publish(); return;
    }
    if (!this.flight || result.requestId !== this.flight.id) { this.pump(); this.publish(); return; }
    const form = this.forms.get(this.flight.key);
    if (!form) { this.flight = null; this.pump(); this.publish(); return; }
    if (result.code !== "ok") {
      form.error = result.message || result.code || "Form request failed";
      form.intent = undefined; form.queued.clear(); this.flight = null;
      this.read(this.keyFromForm(form));
    } else {
      this.flight.revision = result.draftRevision ?? form.revision;
      this.finish();
    }
    this.pump(); this.publish();
  }
  private keyFromForm(form: WorkingForm) { return `${form.form}:${form.generation}`; }
  private stale(form: WorkingForm) {
    form.error = "stale-revision: Form changed while editing; reload fields";
    form.queued.clear(); form.intent = undefined;
  }
  private unavailable(form: WorkingForm) {
    form.error = "stale-generation: Form pane is unavailable; show it and reload fields";
    form.queued.clear(); form.intent = undefined;
  }
  private finish() {
    if (!this.flight?.revision) return;
    const form = this.forms.get(this.flight.key);
    if (!form || BigInt(form.fieldsRevision || "0") < BigInt(this.flight.revision)) return;
    if (form.fieldsRevision !== this.flight.revision && form.queued.size) {
      this.stale(form);
    }
    else for (const value of form.queued.values())
      if (value.base === this.flight.base) value.base = this.flight.revision;
    if (this.flight.field && form.values[this.flight.field] === this.flight.value) delete form.values[this.flight.field];
    this.flight = null;
  }
  private pump() {
    if (this.flight || this.focusFlight || !this.io.available()) return;
    for (const [key, form] of this.forms) {
      if (form.loading || form.fieldsRevision !== form.revision) continue;
      const queued = !form.error ? [...form.queued].find(([, value]) => !value.composing) : undefined;
      if (queued && queued[1].base !== form.revision) { this.stale(form); continue; }
      const intent = !queued && !form.queued.size ? form.intent : undefined;
      if (!queued && !intent) continue;
      const panel = queued ? queued[1].panel : intent!.panel;
      const live = this.livePanel(panel);
      if (!live) { this.unavailable(form); continue; }
      if (!live.focused) {
        if (form.error) { form.intent = undefined; continue; }
        this.requestFocus(panel); return;
      }
      if (intent?.operation === "return") { form.intent = undefined; this.io.returning(panel); return; }
      const operation = queued ? "set" : intent!.operation;
      const id = this.io.nextId();
      this.flight = { id, key, operation, base: form.revision, field: queued?.[0], value: queued?.[1].value };
      if (queued) form.queued.delete(queued[0]); else form.intent = undefined;
      this.io.send({ type: "workbench", version: 1, operation: `form.${operation}`, requestId: id,
        form: form.form, expectGeneration: form.generation, expectRevision: form.revision,
        panel: panel.id, expectPanelGeneration: panel.generation,
        ...(queued ? { field: queued[0], value: queued[1].value } : {}) });
      return;
    }
  }
}
