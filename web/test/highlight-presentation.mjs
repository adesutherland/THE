// Pending paint is presentation only; it must never become parser evidence.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import ts from 'typescript';
const source = await readFile(new URL('../src/highlight-presentation.ts', import.meta.url), 'utf8');
const javascript = ts.transpileModule(source, {compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022}}).outputText;
const {HighlightPresentation} = await import('data:text/javascript;base64,' + Buffer.from(javascript).toString('base64'));
const lexical = [[0, 3, 'keyword'], [4, 8, 'string']];
function snapshot(state = 'ready') {
  return {
    buffer: {path: '/workspace/paint.rexx'}, source: {id: 2, revision: state === 'ready' ? 3 : 4},
    parser: {id: 7, provider: 'DSLSH', providerId: 'classic', state, revision: state === 'ready' ? 5 : 6, applied: state === 'ready' ? 5 : 0},
    screen_rows: [{role: 'file', line: 2, sc: 0, t: "say 'steady'", s: state === 'ready' ? lexical : undefined}],
  };
}
function seed() {const paint = new HighlightPresentation();paint.rows(snapshot());return paint;}
const pending = snapshot('pending');
const retained = seed().rows(pending);
assert.deepEqual(retained[0].s, lexical, 'exact unchanged text retains lexical paint during edited-source parse');
assert.equal(pending.screen_rows[0].s, undefined, 'presentation does not mutate authoritative pending snapshot');
for (const [label, modify] of [
  ['text edit', s => s.screen_rows[0].t += '!'],
  ['horizontal scrolling', s => s.screen_rows[0].sc++],
  ['logical line replacement', s => s.screen_rows[0].line++],
  ['buffer switch', s => s.buffer.path = '/workspace/other.rexx'],
  ['source switch', s => s.source.id++],
  ['parser switch', s => s.parser.id++],
  ['provider switch', s => s.parser.provider = 'Other parser'],
  ['profile language switch', s => s.parser.providerId = 'other-language'],
  ['source-stable parser restart', s => s.source.revision = 3],
  ['old parser revision', s => s.parser.revision = 5],
  ['missing source identity', s => delete s.source],
]) {
  const candidate = snapshot('pending');modify(candidate);
  assert.equal(seed().rows(candidate)[0].s, undefined, label + ' must not reuse paint');
}
for (const state of ['error', 'refused', 'inactive', 'failed']) {
  const paint = seed();
  assert.equal(paint.rows(snapshot(state))[0].s, undefined, state + ' clears displayed paint');
  assert.equal(paint.rows(snapshot('pending'))[0].s, undefined, state + ' discards cache for later pending frames');
}
const paint = new HighlightPresentation(), diagnostic = snapshot();
diagnostic.screen_rows[0].s = [[0, 3, 'keyword'], [4, 8, 'diagnostic-error'], [12, 1, 'diagnostic-warning'], [13, 1, 'diagnostic-information']];
assert.deepEqual(paint.rows(diagnostic)[0].s, diagnostic.screen_rows[0].s, 'ready diagnostic runs remain authoritative');
assert.deepEqual(paint.rows(snapshot('pending'))[0].s, [[0, 3, 'keyword']], 'pending paint never retains diagnostic severity runs');
const replacement = snapshot();replacement.parser.revision = replacement.parser.applied = 6;replacement.source.revision = 4;
replacement.screen_rows[0].s = [[0, 3, 'identifier']];
assert.deepEqual(paint.rows(replacement)[0].s, replacement.screen_rows[0].s, 'completed revision replaces cached styles');
const next = snapshot('pending');next.source.revision = 5;next.parser.revision = 7;
assert.deepEqual(paint.rows(next)[0].s, replacement.screen_rows[0].s, 'next edit uses the latest completed paint');
console.log('PASS highlight presentation: exact unchanged text, source/parser/profile identity, restart/refusal/failure invalidation, no stale diagnostic runs and authoritative replacement');
