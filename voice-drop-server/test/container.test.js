import assert from 'node:assert/strict';
import test from 'node:test';
import { parseVoiceDrop, splitFrames } from '../src/container.js';
import { crc32 } from '../src/crc32.js';
import { buildSpeexOgg } from '../src/ogg-speex.js';

function fixture() {
  const payload = Buffer.from([3, 1, 2, 3, 2, 4, 5]);
  const container = Buffer.alloc(40 + payload.length);
  container.write('VDROP001');
  container.writeUInt32LE(42, 8);
  container.writeUInt32LE(1_700_000_000, 12);
  container.writeUInt32LE(40, 16);
  container.writeUInt32LE(2, 20);
  container.writeUInt32LE(payload.length, 24);
  container.writeUInt32LE(crc32(payload), 28);
  container.writeUInt16LE(16_000, 32);
  container.writeUInt16LE(9_800, 34);
  container.writeUInt16LE(320, 36);
  container[38] = 1;
  container[39] = 4;
  payload.copy(container, 40);
  return container;
}

test('parses and verifies a recording container', () => {
  const recording = parseVoiceDrop(fixture());
  assert.equal(recording.id, 42);
  assert.deepEqual(splitFrames(recording).map((frame) => [...frame]), [[1, 2, 3], [4, 5]]);
});

test('rejects corrupted payload data', () => {
  const container = fixture();
  container[container.length - 1] ^= 1;
  assert.throws(() => parseVoiceDrop(container), /checksum/);
});

test('wraps Speex frames in valid Ogg pages', () => {
  const ogg = buildSpeexOgg(parseVoiceDrop(fixture()));
  assert.equal(ogg.subarray(0, 4).toString(), 'OggS');
  assert.ok(ogg.includes(Buffer.from('Speex   ')));
});
