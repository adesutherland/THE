import type { ScreenRow, Snapshot } from "./types";

export type SelectionSegment = { start: number; end: number | null };

// Cells are supplied by the core, not inferred from UTF-16 string indexes.
// A null end includes the selected line break at the right edge of a row.
export function selectionSegment(selection: Snapshot["selection"], row: ScreenRow, kind: Snapshot["selection_kind"] = "cua"): SelectionSegment | null {
  if (!selection?.active || row.role !== "file" || row.line < 1) return null;
  if (kind !== "cua" && kind !== "line") return null;
  let first = { line: selection.start_line, cell: selection.start_cell - 1 };
  let last = { line: selection.end_line, cell: selection.end_cell - 1 };
  if (![first.line, first.cell, last.line, last.cell].every(Number.isSafeInteger) || first.line < 1 || last.line < 1) return null;
  if (first.line > last.line || first.line === last.line && first.cell > last.cell) [first, last] = [last, first];
  if (row.line < first.line || row.line > last.line) return null;
  if (kind === "line") return { start: 0, end: null };
  if (first.cell < 0 || last.cell < 0) return null;
  const start = row.line === first.line ? first.cell : 0;
  const end = row.line === last.line ? last.cell : null;
  // CUA endpoints are exclusive, including an endpoint at the next line's start.
  if (end !== null && (end <= start || end <= row.sc)) return null;
  return { start: Math.max(0, start - row.sc), end: end === null ? null : end - row.sc };
}
