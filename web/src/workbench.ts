import type { WorkbenchActionDefinition, WorkbenchItemDefinition, WorkbenchMenuDefinition, WorkbenchPage, WorkbenchPanelDefinition } from "./types";

export interface WorkbenchCatalog {
  revision: number;
  actions: WorkbenchActionDefinition[];
  menus: WorkbenchMenuDefinition[];
  items: WorkbenchItemDefinition[];
  panels: WorkbenchPanelDefinition[];
}

export function displayLogText(text: string): string {
  return text.replace(/[\u0000-\u001f\u007f-\u009f]/g, (control) => {
    if (control === "\n") return "\n";
    const code = control.charCodeAt(0);
    return code <= 0xff ? `\\x${code.toString(16).padStart(2, "0").toUpperCase()}`
      : `\\u${code.toString(16).padStart(4, "0").toUpperCase()}`;
  });
}

type Section = WorkbenchPage["section"];
type RecordBySection = {
  actions: WorkbenchActionDefinition;
  menus: WorkbenchMenuDefinition;
  items: WorkbenchItemDefinition;
  panels: WorkbenchPanelDefinition;
};

export class WorkbenchPageCollector {
  private revision = 0;
  private publication = 0;
  private entries: { [K in Section]: Map<number, RecordBySection[K]> } = {
    actions: new Map(), menus: new Map(), items: new Map(), panels: new Map(),
  };
  private totals: Partial<Record<Section, number>> = {};

  reset() {
    this.revision = 0;
    this.publication = 0;
    this.entries = { actions: new Map(), menus: new Map(), items: new Map(), panels: new Map() };
    this.totals = {};
  }

  accept(page: WorkbenchPage, publication = 0): WorkbenchCatalog | null {
    if (page.schemaVersion !== 1 || !["actions", "menus", "items", "panels"].includes(page.section)
        || !Number.isSafeInteger(page.revision) || page.revision < this.revision
        || !Number.isSafeInteger(publication) || publication < 0
        || (publication === 0 && this.publication > 0)
        || (publication > 0 && publication < this.publication)
        || !Number.isSafeInteger(page.offset) || page.offset < 0
        || !Number.isSafeInteger(page.total) || page.total < 0
        || !Array.isArray(page.records) || page.records.length > 8
        || page.offset + page.records.length > page.total) return null;
    if (page.revision > this.revision || publication > this.publication) {
      this.reset();
      this.publication = publication;
    }
    this.revision = page.revision;
    const section = page.section;
    if (this.totals[section] !== undefined && this.totals[section] !== page.total) return null;
    this.totals[section] = page.total;
    // JSON pages are immutable published snapshots. A second page for the same
    // offset may be a transport retry; it must not duplicate any menu entry.
    const entries = this.entries[section] as Map<number, RecordBySection[Section]>;
    page.records.forEach((record, index) => entries.set(page.offset + index, record));
    if (["actions", "menus", "items", "panels"].some((part) =>
      this.totals[part as Section] === undefined || this.entries[part as Section].size !== this.totals[part as Section])) return null;
    return {
      revision: this.revision,
      actions: [...this.entries.actions.entries()].sort(([a], [b]) => a - b).map(([, value]) => value),
      menus: [...this.entries.menus.entries()].sort(([a], [b]) => a - b).map(([, value]) => value),
      items: [...this.entries.items.entries()].sort(([a], [b]) => a - b).map(([, value]) => value),
      panels: [...this.entries.panels.entries()].sort(([a], [b]) => a - b).map(([, value]) => value),
    };
  }
}
