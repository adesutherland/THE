import assert from 'node:assert/strict';
import {FramedInput} from '../vm/classic-host.mjs';
const encode = text => new TextEncoder().encode(text);
const input = new FramedInput();
const first = input.reply();
for (const byte of encode('000000037|x000000010')) input.write(Uint8Array.of(byte));
assert.equal(await first, '7|x');
assert.equal(await input.reply(), '0');
for (const header of ['bad frame', '00800001', '00000000']) {
  const invalid = new FramedInput(); invalid.write(encode(header));
  await assert.rejects(invalid.reply(), /frame|limit/i);
}
const truncated = new FramedInput();
truncated.write(encode('00000003x')); truncated.input.close();
await assert.rejects(truncated.reply(), /incomplete/i);
console.log('PASS Classic host framing: split headers/bodies, concatenated frames, refusal, oversize and truncated EOF');
