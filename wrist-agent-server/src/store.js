import {
  createCipheriv,
  createDecipheriv,
  createHash,
  createHmac,
  randomBytes,
  timingSafeEqual,
} from 'node:crypto';
import { mkdir, readFile, readdir, rename, unlink, writeFile } from 'node:fs/promises';
import path from 'node:path';

const REQUEST_ID_PATTERN = /^wrq_[A-Za-z0-9_-]{12,64}$/;
const IDEMPOTENCY_HASH_PATTERN = /^[a-f0-9]{64}$/;

export function sha256(value) {
  return createHash('sha256').update(String(value), 'utf8').digest('hex');
}

export function capabilityHash(pepper, token) {
  return createHmac('sha256', pepper).update(String(token), 'utf8').digest('hex');
}

export function safeEqualHex(left, right) {
  if (typeof left !== 'string' || typeof right !== 'string' ||
      left.length !== right.length || !/^[a-f0-9]+$/i.test(left + right)) {
    return false;
  }
  return timingSafeEqual(Buffer.from(left, 'hex'), Buffer.from(right, 'hex'));
}

function encryptionKey(secret) {
  return createHash('sha256').update(String(secret), 'utf8').digest();
}

export function sealSecret(secret, plaintext) {
  const iv = randomBytes(12);
  const cipher = createCipheriv('aes-256-gcm', encryptionKey(secret), iv);
  const ciphertext = Buffer.concat([
    cipher.update(String(plaintext), 'utf8'),
    cipher.final(),
  ]);
  const tag = cipher.getAuthTag();
  return [iv, tag, ciphertext].map((value) => value.toString('base64url')).join('.');
}

export function openSecret(secret, sealed) {
  const parts = String(sealed || '').split('.');
  if (parts.length !== 3) {
    throw new Error('invalid sealed secret');
  }
  const [iv, tag, ciphertext] = parts.map((value) => Buffer.from(value, 'base64url'));
  const decipher = createDecipheriv('aes-256-gcm', encryptionKey(secret), iv);
  decipher.setAuthTag(tag);
  return Buffer.concat([
    decipher.update(ciphertext),
    decipher.final(),
  ]).toString('utf8');
}

async function readJson(filePath) {
  try {
    return JSON.parse(await readFile(filePath, 'utf8'));
  } catch (error) {
    if (error && error.code === 'ENOENT') {
      return null;
    }
    throw error;
  }
}

async function atomicWriteJson(filePath, value) {
  const temporaryPath = `${filePath}.${process.pid}.${Date.now()}.tmp`;
  await writeFile(temporaryPath, `${JSON.stringify(value, null, 2)}\n`, {
    encoding: 'utf8',
    mode: 0o600,
  });
  await rename(temporaryPath, filePath);
}

export class RequestStore {
  constructor({ directory, capabilityPepper, now = () => Date.now() }) {
    this.directory = directory;
    this.requestsDirectory = path.join(directory, 'requests');
    this.idempotencyDirectory = path.join(directory, 'idempotency');
    this.capabilityPepper = capabilityPepper;
    this.now = now;
    this.locks = new Map();
  }

  async init() {
    await mkdir(this.requestsDirectory, { recursive: true, mode: 0o700 });
    await mkdir(this.idempotencyDirectory, { recursive: true, mode: 0o700 });
  }

  requestPath(requestId) {
    if (!REQUEST_ID_PATTERN.test(requestId)) {
      throw new Error('invalid request id');
    }
    return path.join(this.requestsDirectory, `${requestId}.json`);
  }

  idempotencyPath(hash) {
    if (!IDEMPOTENCY_HASH_PATTERN.test(hash)) {
      throw new Error('invalid idempotency hash');
    }
    return path.join(this.idempotencyDirectory, `${hash}.json`);
  }

  async withLock(key, callback) {
    const previous = this.locks.get(key) || Promise.resolve();
    let release;
    const current = new Promise((resolve) => {
      release = resolve;
    });
    const queued = previous.then(() => current);
    this.locks.set(key, queued);
    await previous;
    try {
      return await callback();
    } finally {
      release();
      if (this.locks.get(key) === queued) {
        this.locks.delete(key);
      }
    }
  }

  async get(requestId) {
    return readJson(this.requestPath(requestId));
  }

  async getByIdempotency(idempotencyHash) {
    const mapping = await readJson(this.idempotencyPath(idempotencyHash));
    if (!mapping) {
      return null;
    }
    const request = await this.get(mapping.requestId);
    return request ? { mapping, request } : null;
  }

  async create(request, idempotency) {
    return this.withLock(`idempotency:${idempotency.hash}`, async () => {
      const existing = await this.getByIdempotency(idempotency.hash);
      if (existing) {
        return { created: false, ...existing };
      }
      const storedRequest = {
        ...request,
        idempotencyHash: idempotency.hash,
      };
      await atomicWriteJson(this.requestPath(request.requestId), storedRequest);
      const mapping = {
        schemaVersion: 1,
        requestId: request.requestId,
        principalHash: request.principalHash,
        bodyHash: idempotency.bodyHash,
        createdAt: this.now(),
      };
      await atomicWriteJson(this.idempotencyPath(idempotency.hash), mapping);
      return { created: true, request: storedRequest, mapping };
    });
  }

  async claimTrigger(requestId, leaseMs) {
    const duration = Number(leaseMs);
    if (!Number.isSafeInteger(duration) || duration < 1000) {
      throw new Error('trigger lease must be at least one second');
    }

    return this.withLock(`request:${requestId}`, async () => {
      const request = await this.get(requestId);
      if (!request) {
        return { claimed: false, leaseId: null, request: null };
      }

      const timestamp = this.now();
      const leaseIsActive = request.status === 'triggering' &&
        Number(request.triggerLeaseExpiresAt || 0) > timestamp;
      const triggerable = ['trigger_pending', 'trigger_retryable'].includes(request.status) ||
        (request.status === 'triggering' && !leaseIsActive);
      if (!triggerable || request.expiresAt <= timestamp) {
        return { claimed: false, leaseId: null, request };
      }

      const leaseId = randomBytes(18).toString('base64url');
      request.status = 'triggering';
      request.triggerLeaseId = leaseId;
      request.triggerLeaseExpiresAt = timestamp + duration;
      request.updatedAt = timestamp;
      await atomicWriteJson(this.requestPath(requestId), request);
      return { claimed: true, leaseId, request };
    });
  }

  async update(requestId, updater) {
    return this.withLock(`request:${requestId}`, async () => {
      const request = await this.get(requestId);
      if (!request) {
        return null;
      }
      const updated = await updater(structuredClone(request));
      if (!updated) {
        return request;
      }
      updated.updatedAt = this.now();
      await atomicWriteJson(this.requestPath(requestId), updated);
      return updated;
    });
  }

  verifyCapability(request, token) {
    const actual = capabilityHash(this.capabilityPepper, token);
    return safeEqualHex(request.callbackCapabilityHash, actual);
  }

  async cleanup(retentionMs) {
    const names = await readdir(this.requestsDirectory);
    const cutoff = this.now() - retentionMs;
    let removed = 0;
    for (const name of names) {
      if (!name.endsWith('.json')) {
        continue;
      }
      const filePath = path.join(this.requestsDirectory, name);
      const request = await readJson(filePath);
      if (request && request.updatedAt < cutoff) {
        await unlink(filePath);
        removed += 1;
      }
    }

    const mappingNames = await readdir(this.idempotencyDirectory);
    for (const name of mappingNames) {
      if (!name.endsWith('.json')) {
        continue;
      }
      const filePath = path.join(this.idempotencyDirectory, name);
      const mapping = await readJson(filePath);
      const request = mapping && REQUEST_ID_PATTERN.test(mapping.requestId)
        ? await readJson(this.requestPath(mapping.requestId))
        : null;
      if (!request || request.updatedAt < cutoff) {
        await unlink(filePath);
        removed += 1;
      }
    }
    return removed;
  }
}
