/** A relative portable snapshot basename; logical identity stays with THE. */
export function sourceFilename(filename) {
  let name = String(filename ?? '').split(/[/\\]/).at(-1);
  if (!name || new TextEncoder().encode(name).length > 240 || name === '.' || name === '..') name = 'source';
  name = name.replace(/[\x00-\x1f<>:"/\\|?*]/g, '_').replace(/[ .]+$/, value => '_'.repeat(value.length));
  if (/^(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)/i.test(name)) name = '_' + name;
  return name;
}
