import { useLayoutEffect, useRef } from "preact/hooks";
import type { WorkbenchPanelDefinition } from "./types";
import type { FormView, WorkbenchForms } from "./workbench-forms";
import { displayLogText } from "./workbench";

export type FormViewport = { top: number; left: number; field: string; start: number; end: number };

export function NamedFormPane({ panel, view, forms, focusRef, viewport }: {
  panel: WorkbenchPanelDefinition; view?: FormView; forms: WorkbenchForms;
  focusRef: (node: HTMLDivElement | null) => void;
  viewport: FormViewport;
}) {
  const composing = useRef(new Set<string>());
  const root = useRef<HTMLDivElement | null>(null);
  const restore = () => {
    const input = root.current?.querySelector<HTMLInputElement>(`input[data-field-id="${CSS.escape(viewport.field)}"]:not(:disabled)`)
      ?? root.current?.querySelector<HTMLInputElement>("input:not(:disabled)");
    if (input) { input.focus(); if (input.dataset.fieldId === viewport.field) input.setSelectionRange(viewport.start, viewport.end); }
  };
  useLayoutEffect(() => {
    const node = root.current;
    if (!node) return;
    const start = (event: Event) => {
      if (event.target instanceof HTMLInputElement && event.target.dataset.fieldId) {
        composing.current.add(event.target.dataset.fieldId);
        forms.change(panel, event.target.dataset.fieldId, event.target.value, true);
      }
    };
    const end = (event: Event) => {
      if (event.target instanceof HTMLInputElement && event.target.dataset.fieldId) {
        const field = event.target.dataset.fieldId;
        composing.current.delete(field); forms.change(panel, field, event.target.value, false);
      }
    };
    node.addEventListener("compositionstart", start); node.addEventListener("compositionend", end);
    return () => { node.removeEventListener("compositionstart", start); node.removeEventListener("compositionend", end); composing.current.clear(); };
  }, [panel.id, panel.generation]);
  useLayoutEffect(() => { if (document.activeElement === root.current) restore(); }, [view?.fields.length]);
  return <div class="named-form-body" ref={(node) => {
    root.current = node; focusRef(node);
    if (node) { node.scrollTop = viewport.top; node.scrollLeft = viewport.left; }
  }} tabIndex={0} aria-label={`${panel.title} form`}
    onScroll={(event) => { viewport.top = event.currentTarget.scrollTop; viewport.left = event.currentTarget.scrollLeft; }}
    onFocus={(event) => {
      if (event.target === event.currentTarget) restore();
      else if (event.target instanceof HTMLInputElement) viewport.field = event.target.dataset.fieldId ?? "";
    }}>
    <p class="named-form-status" role="status">{view?.error || (view?.loading ? "Loading fields…"
      : view?.dirty ? "Unsubmitted changes" : view?.submitted ? "Submitted" : "Ready")}</p>
    <form onSubmit={(event) => { event.preventDefault(); forms.intent(panel, "submit"); }}
      onFocus={() => forms.focus(panel)}>
      <div class="named-form-fields">
        {view?.fields.map((field) => <label class="named-form-field" key={`${field.id}:${field.generation}`}>
          <span>{field.label}</span>
          {field.access === "edit" ? <input type="text" data-field-id={field.id}
            aria-label={field.label} disabled={!!view.error || view.locked && !composing.current.has(field.id)}
            value={view.values[field.id] ?? field.value} autoComplete="off" spellcheck={false}
            onInput={(event) => forms.change(panel, field.id, event.currentTarget.value, composing.current.has(field.id))}
            onSelect={(event) => { viewport.field = field.id; viewport.start = event.currentTarget.selectionStart ?? 0; viewport.end = event.currentTarget.selectionEnd ?? 0; }}
            />
            : <output data-field-id={field.id} aria-label={`${field.label} read only`}>{displayLogText(field.value)}</output>}
        </label>)}
      </div>
      <div class="named-form-controls">
        <button type="submit" disabled={!view || view.loading || !!view.error || view.locked}>Submit</button>
        <button type="button" disabled={!view || view.loading || view.locked} onClick={() => forms.intent(panel, "cancel")}>Cancel</button>
        <button type="button" onClick={() => forms.intent(panel, "return")}>Return</button>
        {view?.error ? <button type="button" onClick={() => forms.reload(panel)}>Reload fields</button> : null}
      </div>
    </form>
  </div>;
}
