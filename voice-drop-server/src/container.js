import { crc32 } from './crc32.js';

const MAGIC = Buffer.from('VDROP001');
export const HEADER_SIZE = 40;
export const MAX_PAYLOAD_SIZE = 8 * 1024 * 1024;

export function parseVoiceDrop(buffer) {
  if (!Buffer.isBuffer(buffer) || buffer.length < HEADER_SIZE) {
    throw new Error('Voice Drop container is truncated');
  }
  if (!buffer.subarray(0, MAGIC.length).equals(MAGIC)) {
    throw new Error('Voice Drop magic is invalid');
  }
  const payloadSize = buffer.readUInt32LE(24);
  if (payloadSize === 0 || payloadSize > MAX_PAYLOAD_SIZE ||
      buffer.length !== HEADER_SIZE + payloadSize) {
    throw new Error('Voice Drop payload size is invalid');
  }
  const payload = buffer.subarray(HEADER_SIZE);
  const expectedCrc = buffer.readUInt32LE(28);
  if (crc32(payload) !== expectedCrc) {
    throw new Error('Voice Drop payload checksum mismatch');
  }
  const recording = {
    id: buffer.readUInt32LE(8),
    startedAt: buffer.readUInt32LE(12),
    durationMs: buffer.readUInt32LE(16),
    frameCount: buffer.readUInt32LE(20),
    payloadSize,
    payloadCrc32: expectedCrc,
    sampleRate: buffer.readUInt16LE(32),
    bitRate: buffer.readUInt16LE(34),
    frameSize: buffer.readUInt16LE(36),
    channels: buffer.readUInt8(38),
    bitstreamVersion: buffer.readUInt8(39),
    payload,
  };
  validateMetadata(recording);
  return recording;
}

function validateMetadata(recording) {
  if (!recording.id || recording.sampleRate !== 16000 ||
      recording.channels < 1 || recording.channels > 2 ||
      recording.frameSize === 0 || recording.frameSize > 4096 ||
      recording.frameCount === 0) {
    throw new Error('Voice Drop metadata is invalid');
  }
}

export function splitFrames(recording) {
  const frames = [];
  let offset = 0;
  while (offset < recording.payload.length) {
    const size = recording.payload[offset++];
    if (size === 0 || offset + size > recording.payload.length) {
      throw new Error('Voice Drop frame stream is invalid');
    }
    frames.push(recording.payload.subarray(offset, offset + size));
    offset += size;
  }
  if (frames.length !== recording.frameCount) {
    throw new Error('Voice Drop frame count mismatch');
  }
  return frames;
}
