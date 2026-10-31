import { useLayoutEffect, useRef } from "preact/hooks";
import type { WorkbenchPanelDefinition } from "./types";
import type { FileFieldView, WorkbenchFileFields } from "./workbench-file-fields";
import type { FormViewport } from "./form-pane";
import { displayLogText } from "./workbench";

export function FileFieldPane({ panel, view, fields, focusRef, viewport }: {
  panel: WorkbenchPanelDefinition; view?: FileFieldView; fields: WorkbenchFileFields;
  focusRef: (node: HTMLDivElement | null) => void; viewport: FormViewport;
}) {
  const root = useRef<HTMLDivElement | null>(null);
  const composing = useRef(new Set<string>());
  const appliedCaret = useRef(new Map<string, number>());
  const restore = () => {
    const input = root.current?.querySelector<HTMLInputElement>(`input[data-field-id="${CSS.escape(viewport.field)}"]:not(:disabled)`)
      ?? root.current?.querySelector<HTMLInputElement>("input:not(:disabled)");
    if (input) {
      input.focus({ preventScroll: true });
      if (input.dataset.fieldId === viewport.field) input.setSelectionRange(viewport.start, viewport.end);
    }
  };
  useLayoutEffect(() => {
    const node = root.current; if (!node) return;
    const start = (event: Event) => {
      if (event.target instanceof HTMLInputElement && event.target.dataset.fieldId) {
        composing.current.add(event.target.dataset.fieldId);
        fields.change(panel, event.target.dataset.fieldId, event.target.value, true);
      }
    };
    const end = (event: Event) => {
      if (event.target instanceof HTMLInputElement && event.target.dataset.fieldId) {
        composing.current.delete(event.target.dataset.fieldId);
        fields.change(panel, event.target.dataset.fieldId, event.target.value, false);
      }
    };
    node.addEventListener("compositionstart", start); node.addEventListener("compositionend", end);
    node.addEventListener("focus", restore);
    return () => { node.removeEventListener("compositionstart", start); node.removeEventListener("compositionend", end); node.removeEventListener("focus", restore); composing.current.clear(); };
  }, [panel.id, panel.generation]);
  useLayoutEffect(() => { if (document.activeElement === root.current) restore(); }, [view?.rows.length, panel.focused]);
  useLayoutEffect(() => {
    for (const [field, caret] of Object.entries(view?.carets ?? {})) {
      if (appliedCaret.current.get(field) === caret.serial || composing.current.has(field)) continue;
      const input = root.current?.querySelector<HTMLInputElement>(`input[data-field-id="${CSS.escape(field)}"]`);
      if (input && document.activeElement === input) input.setSelectionRange(caret.offset, caret.offset);
      appliedCaret.current.set(field, caret.serial);
    }
  }, [view]);
  const beforeInput = (event: InputEvent & { currentTarget: HTMLInputElement }, field: string) => {
    const input = event.currentTarget;
    if (composing.current.has(field) || event.isComposing || event.inputType === "insertCompositionText") return;
    const start = input.selectionStart ?? 0, end = input.selectionEnd ?? start;
    // Replacing a complete value is SET. A caret edit goes through the core's
    // fixed-cell operation; UTF-16 offsets are interpreted only by the owner.
    if (start === 0 && end === input.value.length && start !== end) return;
    if (start !== end) { event.preventDefault(); fields.selectionError(panel); return; }
    const edit = event.inputType === "deleteContentBackward" ? "backspace"
      : event.inputType === "deleteContentForward" ? "delete" : undefined;
    if (edit) { event.preventDefault(); fields.edit(panel, field, edit, start); return; }
    if (event.inputType === "insertText" && event.data && Array.from(event.data).length === 1) {
      event.preventDefault(); fields.edit(panel, field, "insert", start, event.data.codePointAt(0)); return;
    }
    // Bulk text belongs to whole-value/IME SET. Partial paste, cut and browser
    // history operations have no qualified fixed-cell operation in this phase.
    event.preventDefault(); fields.selectionError(panel);
  };
  const retainScroll = (node: HTMLDivElement | null) => {
    root.current = node; focusRef(node);
    if (node) { node.scrollTop = viewport.top; node.scrollLeft = viewport.left; }
  };
  return <div class="file-field-body">
    <p class="file-field-status" role="status">{view?.error || view?.notice || (view?.loading ? "Loading file fields…"
      : !view?.active ? "Projection closed · Reload to capture current file"
      : view.dirty || Object.keys(view.values).length ? "Staged changes · Apply changes the buffer; Save remains explicit"
      : "File fields · Save remains explicit")}</p>
    <div class="file-field-grid" ref={retainScroll} tabIndex={0} aria-label={`${panel.title} file fields`}
      onScroll={(event) => { viewport.top = event.currentTarget.scrollTop; viewport.left = event.currentTarget.scrollLeft; }}
      onFocus={(event) => {
        fields.focus(panel);
        if (event.target === event.currentTarget) restore();
        else if (event.target instanceof HTMLInputElement) viewport.field = event.target.dataset.fieldId ?? "";
      }}>
      {view?.rows.map((row) => <div class="file-field-row" key={row.line} data-line={row.line}
        style={{ width: `${Math.max(1, row.cells)}ch` }} aria-label={`Line ${row.line}`}>
        {row.spans.map((span, index) => <span key={`${span.id}:${index}`}
          class={`file-field-span ${span.editable ? "file-field-editable" : "file-field-protected"}`}
          style={{ left: `${span.cell}ch`, width: `${span.maxcells}ch` }}>
          {span.editable ? <input type="text" data-field-id={span.id} aria-label={`Line ${row.line}, field ${span.id}`}
            value={view.values[span.id] ?? span.value} autoComplete="off" spellcheck={false}
            disabled={!!view.error || !view.active || view.locked && !composing.current.has(span.id)}
            onBeforeInput={(event) => beforeInput(event as InputEvent & { currentTarget: HTMLInputElement }, span.id)}
            onInput={(event) => fields.change(panel, span.id, event.currentTarget.value, composing.current.has(span.id))}
            onSelect={(event) => { viewport.field = span.id; viewport.start = event.currentTarget.selectionStart ?? 0; viewport.end = event.currentTarget.selectionEnd ?? 0; }} />
            : <span aria-label="Protected text">{displayLogText(span.value)}</span>}
        </span>)}
      </div>)}
    </div>
    <div class="file-field-controls" role="group" aria-label="File field actions">
      <button type="button" disabled={!view || view.loading || !!view.error || view.locked || !view.active}
        onClick={() => fields.intent(panel, "apply")}>Apply</button>
      <button type="button" disabled={!view || view.loading || view.locked}
        onClick={() => fields.intent(panel, "cancel")}>Cancel</button>
      <button type="button" disabled={!view || view.loading || view.locked}
        onClick={() => fields.intent(panel, "return")}>Return</button>
      <button type="button" disabled={!view || view.loading || view.locked}
        onClick={() => fields.intent(panel, "reload")}>Reload</button>
    </div>
  </div>;
}
