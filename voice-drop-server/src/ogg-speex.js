import { splitFrames } from './container.js';

const OGG_CRC_TABLE = new Uint32Array(256);
for (let i = 0; i < OGG_CRC_TABLE.length; i += 1) {
  let value = i << 24;
  for (let bit = 0; bit < 8; bit += 1) {
    value = value & 0x80000000 ? (value << 1) ^ 0x04c11db7 : value << 1;
  }
  OGG_CRC_TABLE[i] = value >>> 0;
}

export function buildSpeexOgg(recording, serial = recording.id) {
  const packets = [speexHeader(recording), commentPacket(), ...splitFrames(recording)];
  const pages = [];
  let granule = 0n;
  for (let index = 0; index < packets.length; index += 1) {
    if (index >= 2) granule += BigInt(recording.frameSize);
    const headerType = (index === 0 ? 0x02 : 0) |
      (index === packets.length - 1 ? 0x04 : 0);
    pages.push(oggPage(packets[index], headerType, granule, serial, index));
  }
  return Buffer.concat(pages);
}

function speexHeader(recording) {
  const header = Buffer.alloc(80);
  header.write('Speex   ', 0, 'ascii');
  header.write('speex-1.2', 8, 'ascii');
  header.writeInt32LE(1, 28);
  header.writeInt32LE(80, 32);
  header.writeInt32LE(recording.sampleRate, 36);
  header.writeInt32LE(recording.sampleRate > 8000 ? 1 : 0, 40);
  header.writeInt32LE(recording.bitstreamVersion, 44);
  header.writeInt32LE(recording.channels, 48);
  header.writeInt32LE(recording.bitRate, 52);
  header.writeInt32LE(recording.frameSize, 56);
  header.writeInt32LE(0, 60);
  header.writeInt32LE(1, 64);
  return header;
}

function commentPacket() {
  const vendor = Buffer.from('Voice Drop');
  const packet = Buffer.alloc(8 + vendor.length);
  packet.writeUInt32LE(vendor.length, 0);
  vendor.copy(packet, 4);
  packet.writeUInt32LE(0, 4 + vendor.length);
  return packet;
}

function oggPage(packet, headerType, granule, serial, sequence) {
  if (packet.length >= 255) throw new Error('Oversized Speex packet');
  const page = Buffer.alloc(28 + packet.length);
  page.write('OggS', 0, 'ascii');
  page[4] = 0;
  page[5] = headerType;
  page.writeBigUInt64LE(granule, 6);
  page.writeUInt32LE(serial >>> 0, 14);
  page.writeUInt32LE(sequence >>> 0, 18);
  page.writeUInt32LE(0, 22);
  page[26] = 1;
  page[27] = packet.length;
  packet.copy(page, 28);
  page.writeUInt32LE(oggCrc(page), 22);
  return page;
}

function oggCrc(buffer) {
  let crc = 0;
  for (const byte of buffer) {
    crc = ((crc << 8) ^ OGG_CRC_TABLE[((crc >>> 24) ^ byte) & 0xff]) >>> 0;
  }
  return crc;
}
