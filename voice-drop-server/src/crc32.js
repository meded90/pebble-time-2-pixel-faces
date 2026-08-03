const TABLE = new Uint32Array(256);
for (let i = 0; i < TABLE.length; i += 1) {
  let value = i;
  for (let bit = 0; bit < 8; bit += 1) {
    value = (value & 1) ? (value >>> 1) ^ 0xedb88320 : value >>> 1;
  }
  TABLE[i] = value >>> 0;
}

export function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc = (crc >>> 8) ^ TABLE[(crc ^ byte) & 0xff];
  }
  return (crc ^ 0xffffffff) >>> 0;
}
