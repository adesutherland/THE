// Clipboard transport and DOM targets share the same user action, while the
// editor remains the authority for selected bytes and atomic transactions.
export type EditOperation = "copy" | "cut" | "paste" | "selectAll";
export type ClipboardReply = {
  type: "clipboard"; operation: string; requestId: string; rc: number; code: string;
  token?: string; text?: string; readOnly?: boolean; message?: string;
};
export type EditTarget =
  | { kind: "editor"; element: HTMLElement }
  | { kind: "entry"; element: HTMLInputElement | HTMLTextAreaElement; value: string; start: number; end: number; direction: "forward" | "backward" | "none" }
  | { kind: "output"; element: HTMLElement; range: Range | null; text: string };

export function captureEditTarget(active: Element | null = document.activeElement): EditTarget | null {
  if (!(active instanceof HTMLElement)) return null;
  if (active.matches(".input-sink")) return { kind: "editor", element: active };
  if ((active instanceof HTMLInputElement || active instanceof HTMLTextAreaElement)
      && active.selectionStart !== null && active.selectionEnd !== null)
    return { kind: "entry", element: active, value: active.value,
      start: active.selectionStart, end: active.selectionEnd, direction: active.selectionDirection ?? "none" };
  const output = active.closest<HTMLElement>(".console-output, .build-output, .named-panel-output, .command-responses, output");
  const selection = window.getSelection();
  // Pointer selection can be inside an output whose containing pane owns focus.
  const range = selection?.rangeCount ? selection.getRangeAt(0) : null;
  const selectedOutput = range?.commonAncestorContainer instanceof Element
    ? range.commonAncestorContainer.closest<HTMLElement>(".console-output, .build-output, .named-panel-output, .command-responses, output")
    : range?.commonAncestorContainer.parentElement?.closest<HTMLElement>(".console-output, .build-output, .named-panel-output, .command-responses, output");
  const element = output ?? selectedOutput;
  if (!element) return null;
  const ownedRange = range && element.contains(range.commonAncestorContainer) ? range.cloneRange() : null;
  return { kind: "output", element, range: ownedRange, text: ownedRange ? selection?.toString() ?? "" : "" };
}

export function restoreEditTarget(target: EditTarget | null) {
  if (!target?.element.isConnected || !target.element.getClientRects().length) return;
  target.element.focus({ preventScroll: true });
  if (target.kind === "entry" && target.element.value === target.value
      && target.element.selectionStart === target.start && target.element.selectionEnd === target.end)
    target.element.setSelectionRange(target.start, target.end, target.direction);
  if (target.kind === "output" && target.range) {
    const selection = window.getSelection();
    selection?.removeAllRanges(); selection?.addRange(target.range);
  }
}

const limit = 1024 * 1024;
export function validateClipboardText(text: string) {
  if (text.includes("\0")) throw new Error("Clipboard text contains a NUL character. Nothing was changed.");
  if (new TextEncoder().encode(text).length > limit)
    throw new Error("Clipboard text exceeds the 1 MiB limit. Nothing was changed.");
}

function currentEntry(target: Extract<EditTarget, { kind: "entry" }>) {
  const entry = target.element;
  if (!entry.isConnected || entry.disabled || entry.readOnly) throw new Error("This target is read-only. Nothing was changed.");
  if (entry.value !== target.value || entry.selectionStart !== target.start || entry.selectionEnd !== target.end)
    throw new Error("The target or selection changed while waiting for the clipboard. Nothing was changed.");
  return entry;
}

export async function performEdit(operation: EditOperation, target: EditTarget | null,
  request: (operation: string, values?: Record<string, unknown>) => Promise<ClipboardReply>,
  clipboard: Pick<Clipboard, "readText" | "writeText"> | undefined, pasteText?: string) {
  if (!target) throw new Error("Focus an editor, text field or output selection first.");
  if (operation === "selectAll") {
    if (target.kind === "editor") { await request("selectAll"); return; }
    if (target.kind === "entry") { target.element.focus(); target.element.select(); return; }
    const range = document.createRange(); range.selectNodeContents(target.element);
    const selection = window.getSelection(); selection?.removeAllRanges(); selection?.addRange(range);
    target.element.focus({ preventScroll: true }); return;
  }
  if (operation === "copy" || operation === "cut") {
    if (operation === "cut" && target.kind === "output") throw new Error("Output is read-only. Use Copy to copy the selection.");
    if (operation === "cut" && target.kind === "entry") currentEntry(target);
    const reply = target.kind === "editor" ? await request("copy") : undefined;
    if (operation === "cut" && reply?.readOnly) throw new Error("This document is read-only. Use Copy to copy the selection.");
    const text = reply?.text ?? (target.kind === "entry" ? target.value.slice(target.start, target.end)
      : target.kind === "output" ? target.text : "");
    if (!text) throw new Error("There is no selected text to copy.");
    validateClipboardText(text);
    if (!clipboard?.writeText) throw new Error("Clipboard access is unavailable. Allow clipboard access in this browser.");
    await clipboard.writeText(text);
    if (operation === "cut") {
      if (target.kind === "editor") await request("cut", { token: reply?.token });
      else if (target.kind === "entry") {
        const entry = currentEntry(target); entry.setRangeText("", target.start, target.end, "end");
        entry.dispatchEvent(new InputEvent("input", { bubbles: true, inputType: "deleteByCut" }));
      }
    }
    return;
  }
  if (target.kind === "output") throw new Error("Output is read-only. Focus an editable target to paste.");
  if (target.kind === "entry") currentEntry(target);
  // Start the browser read during the user gesture. The core capture is queued
  // first, and its token still guards against edits during the asynchronous read.
  const capture = target.kind === "editor" ? request("capture") : Promise.resolve(undefined);
  let read: Promise<string>;
  if (pasteText !== undefined) read = Promise.resolve(pasteText);
  else if (clipboard?.readText) read = clipboard.readText();
  else { await capture; throw new Error("Clipboard access is unavailable. Use your browser’s Paste command."); }
  const [reply, text] = await Promise.all([capture, read]);
  validateClipboardText(text);
  if (!text.length) throw new Error("The clipboard contains no text. Nothing was changed.");
  if (target.kind === "editor") await request("paste", { token: reply?.token, text });
  else {
    const entry = currentEntry(target);
    // Single-line browser fields cannot represent line breaks; refuse instead
    // of silently flattening the clipboard. Program input is one submitted record.
    if (entry instanceof HTMLInputElement && /[\r\n]/.test(text))
      throw new Error("This field accepts one line. Paste into a multiline field instead.");
    entry.setRangeText(text.replaceAll("\r\n", "\n").replaceAll("\r", "\n"), target.start, target.end, "end");
    entry.dispatchEvent(new InputEvent("input", { bubbles: true, inputType: "insertFromPaste", data: text }));
  }
}

export class ClipboardRequests {
  private serial = 0;
  private pending = new Map<string, { resolve: (reply: ClipboardReply) => void; reject: (error: Error) => void; timer: number }>();
  constructor(private send: (message: Record<string, unknown>) => number | undefined) {}
  request = (operation: string, values: Record<string, unknown> = {}) => new Promise<ClipboardReply>((resolve, reject) => {
    const requestId = `clipboard.${++this.serial}`;
    const timer = window.setTimeout(() => { this.pending.delete(requestId); reject(new Error("The clipboard response timed out. Check the document before retrying.")); }, 15000);
    this.pending.set(requestId, { resolve, reject, timer });
    if (this.send({ type: "clipboard", version: 1, operation, requestId, ...values }) === undefined) {
      window.clearTimeout(timer); this.pending.delete(requestId); reject(new Error("Editor is offline. Nothing was changed."));
    }
  });
  receive(reply: ClipboardReply) {
    const pending = this.pending.get(reply.requestId);
    if (!pending) return;
    window.clearTimeout(pending.timer); this.pending.delete(reply.requestId);
    if (reply.rc) pending.reject(new Error(reply.message || `${reply.code.replaceAll("-", " ")}. Nothing was changed.`));
    else pending.resolve(reply);
  }
  reset() {
    for (const pending of this.pending.values()) { window.clearTimeout(pending.timer); pending.reject(new Error("Editor disconnected. Check the document before retrying.")); }
    this.pending.clear();
  }
}
