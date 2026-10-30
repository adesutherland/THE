import type { ScreenRow, Snapshot, StyleRun } from "./types";

type PaintedRow = { text: string; startCell: number; styles: StyleRun[] };
type AcceptedPaint = {
  identity: string;
  sourceRevision: number;
  parserRevision: number;
  rows: Map<number, PaintedRow>;
};

// A pending parse withdraws authoritative tokens for the whole document. Keep
// the last paint of unchanged visible text until its replacement arrives,
// without changing parser state, diagnostic evidence or the edited text.
export class HighlightPresentation {
  private accepted: AcceptedPaint | null = null;

  rows(snapshot: Snapshot | null): ScreenRow[] {
    const rows = snapshot?.screen_rows ?? [];
    const parser = snapshot?.parser;
    const source = snapshot?.source;
    if (!parser || !source || !["ready", "pending"].includes(parser.state)) {
      this.accepted = null;
      return rows;
    }
    const identity = JSON.stringify([snapshot?.buffer?.path, source.id,
      parser.id, parser.provider, parser.providerId]);
    if (parser.state === "ready" && parser.applied === parser.revision) {
      this.accepted = {
        identity, sourceRevision: source.revision, parserRevision: parser.revision,
        rows: new Map(rows.filter((row) => row.role === "file" && row.s?.length)
          .map((row) => [row.line, { text: row.t, startCell: row.sc,
            styles: row.s!.filter(([, , style]) => !style.startsWith("diagnostic-")) }])),
      };
      return rows;
    }
    const accepted = this.accepted;
    // A source-stable revision is a parser/profile restart, rather than an edit.
    // Refused/failed/inactive states and identity switches also discard paint.
    if (parser.state !== "pending" || !accepted || accepted.identity !== identity ||
        source.revision === accepted.sourceRevision || parser.revision <= accepted.parserRevision) {
      this.accepted = null;
      return rows;
    }
    return rows.map((row) => {
      const previous = accepted.rows.get(row.line);
      return row.role === "file" && !row.s?.length && previous &&
        previous.text === row.t && previous.startCell === row.sc
        ? { ...row, s: previous.styles } : row;
    });
  }
}
