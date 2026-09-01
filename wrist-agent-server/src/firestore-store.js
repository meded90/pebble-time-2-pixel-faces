import { randomBytes } from 'node:crypto';
import { Firestore } from '@google-cloud/firestore';
import { capabilityHash, safeEqualHex } from './store.js';

const REQUEST_ID_PATTERN = /^wrq_[A-Za-z0-9_-]{12,64}$/;
const IDEMPOTENCY_HASH_PATTERN = /^[a-f0-9]{64}$/;
const COLLECTION_PREFIX_PATTERN = /^[A-Za-z][A-Za-z0-9_-]{2,60}$/;

function clone(value) {
  return structuredClone(value);
}

function snapshotData(snapshot) {
  return snapshot.exists ? clone(snapshot.data()) : null;
}

function requireRequestId(requestId) {
  if (!REQUEST_ID_PATTERN.test(requestId)) {
    throw new Error('invalid request id');
  }
  return requestId;
}

function requireIdempotencyHash(hash) {
  if (!IDEMPOTENCY_HASH_PATTERN.test(hash)) {
    throw new Error('invalid idempotency hash');
  }
  return hash;
}

function requireCollectionPrefix(prefix) {
  if (!COLLECTION_PREFIX_PATTERN.test(prefix)) {
    throw new Error('invalid Firestore collection prefix');
  }
  return prefix;
}

function purgeDate(updatedAt, retentionMs) {
  return new Date(updatedAt + retentionMs);
}

/**
 * Persistent store for Cloud Functions Gen 2.
 *
 * All application timestamps remain epoch milliseconds. `purgeAt` is the only
 * Firestore Timestamp-compatible field and is consumed by the TTL policies
 * configured by scripts/deploy-google-cloud-function.sh.
 */
export class FirestoreRequestStore {
  constructor({
    capabilityPepper,
    collectionPrefix = 'wrist_agent',
    retentionMs = 86400000,
    now = () => Date.now(),
    firestore = new Firestore(),
  }) {
    if (!Number.isSafeInteger(retentionMs) || retentionMs < 1) {
      throw new Error('retentionMs must be a positive integer');
    }
    this.firestore = firestore;
    this.capabilityPepper = capabilityPepper;
    this.collectionPrefix = requireCollectionPrefix(collectionPrefix);
    this.requestsCollectionName = `${this.collectionPrefix}_requests`;
    this.idempotencyCollectionName = `${this.collectionPrefix}_idempotency`;
    this.retentionMs = retentionMs;
    this.now = now;
    this.locks = new Map();
  }

  requests() {
    return this.firestore.collection(this.requestsCollectionName);
  }

  idempotency() {
    return this.firestore.collection(this.idempotencyCollectionName);
  }

  requestRef(requestId) {
    return this.requests().doc(requireRequestId(requestId));
  }

  idempotencyRef(hash) {
    return this.idempotency().doc(requireIdempotencyHash(hash));
  }

  async init() {
    // Collections are created lazily. This read validates Application Default
    // Credentials, the Firestore database, and the runtime IAM role at cold start.
    await this.requests().limit(1).get();
  }

  // Kept for compatibility with the file store. Cross-instance critical paths
  // use claimTrigger(), which is backed by a Firestore transaction instead.
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
    const snapshot = await this.requestRef(requestId).get();
    return snapshotData(snapshot);
  }

  async getByIdempotency(idempotencyHash) {
    const mappingSnapshot = await this.idempotencyRef(idempotencyHash).get();
    const mapping = snapshotData(mappingSnapshot);
    if (!mapping) {
      return null;
    }
    const request = await this.get(mapping.requestId);
    return request ? { mapping, request } : null;
  }

  async create(request, idempotency) {
    const requestRef = this.requestRef(request.requestId);
    const mappingRef = this.idempotencyRef(idempotency.hash);
    const createdAt = this.now();
    const retentionMs = this.retentionMs;

    return this.firestore.runTransaction(async (transaction) => {
      const [mappingSnapshot, requestSnapshot] = await Promise.all([
        transaction.get(mappingRef),
        transaction.get(requestRef),
      ]);
      const mapping = snapshotData(mappingSnapshot);
      if (mapping) {
        const existingRequestSnapshot = await transaction.get(
          this.requestRef(mapping.requestId));
        const existingRequest = snapshotData(existingRequestSnapshot);
        if (existingRequest) {
          return { created: false, mapping, request: existingRequest };
        }
      }
      if (requestSnapshot.exists) {
        throw new Error('request id already exists');
      }

      const storedRequest = {
        ...clone(request),
        idempotencyHash: idempotency.hash,
        purgeAt: purgeDate(request.updatedAt || createdAt, retentionMs),
      };
      const storedMapping = {
        schemaVersion: 1,
        requestId: request.requestId,
        principalHash: request.principalHash,
        bodyHash: idempotency.bodyHash,
        createdAt,
        purgeAt: purgeDate(request.updatedAt || createdAt, retentionMs),
      };
      transaction.set(requestRef, storedRequest);
      transaction.set(mappingRef, storedMapping);
      return { created: true, mapping: storedMapping, request: storedRequest };
    });
  }

  async update(requestId, updater) {
    const requestRef = this.requestRef(requestId);
    const retentionMs = this.retentionMs;
    return this.firestore.runTransaction(async (transaction) => {
      const snapshot = await transaction.get(requestRef);
      const current = snapshotData(snapshot);
      if (!current) {
        return null;
      }

      const updated = await updater(clone(current));
      if (!updated) {
        return current;
      }

      const mappingRef = IDEMPOTENCY_HASH_PATTERN.test(updated.idempotencyHash || '')
        ? this.idempotencyRef(updated.idempotencyHash)
        : null;
      const mappingSnapshot = mappingRef ? await transaction.get(mappingRef) : null;
      const timestamp = this.now();
      updated.updatedAt = timestamp;
      updated.purgeAt = purgeDate(timestamp, retentionMs);
      transaction.set(requestRef, updated);
      if (mappingRef && mappingSnapshot?.exists) {
        const mapping = snapshotData(mappingSnapshot);
        mapping.purgeAt = purgeDate(timestamp, retentionMs);
        transaction.set(mappingRef, mapping);
      }
      return updated;
    });
  }

  async claimTrigger(requestId, leaseMs) {
    const duration = Number(leaseMs);
    if (!Number.isSafeInteger(duration) || duration < 1000) {
      throw new Error('trigger lease must be at least one second');
    }

    const requestRef = this.requestRef(requestId);
    const retentionMs = this.retentionMs;
    return this.firestore.runTransaction(async (transaction) => {
      const snapshot = await transaction.get(requestRef);
      const request = snapshotData(snapshot);
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

      const mappingRef = IDEMPOTENCY_HASH_PATTERN.test(request.idempotencyHash || '')
        ? this.idempotencyRef(request.idempotencyHash)
        : null;
      const mappingSnapshot = mappingRef ? await transaction.get(mappingRef) : null;
      const leaseId = randomBytes(18).toString('base64url');
      request.status = 'triggering';
      request.triggerLeaseId = leaseId;
      request.triggerLeaseExpiresAt = timestamp + duration;
      request.updatedAt = timestamp;
      request.purgeAt = purgeDate(timestamp, retentionMs);
      transaction.set(requestRef, request);
      if (mappingRef && mappingSnapshot?.exists) {
        const mapping = snapshotData(mappingSnapshot);
        mapping.purgeAt = purgeDate(timestamp, retentionMs);
        transaction.set(mappingRef, mapping);
      }
      return { claimed: true, leaseId, request };
    });
  }

  verifyCapability(request, token) {
    const actual = capabilityHash(this.capabilityPepper, token);
    return safeEqualHex(request.callbackCapabilityHash, actual);
  }

  async cleanup(retentionMs = this.retentionMs) {
    const cutoff = this.now() - retentionMs;
    const stale = await this.requests().where('updatedAt', '<', cutoff).limit(50).get();
    let removed = 0;
    for (const candidate of stale.docs) {
      // Re-read transactionally so a stale query snapshot can never delete a
      // request renewed by another Cloud Functions instance.
      removed += await this.firestore.runTransaction(async (transaction) => {
        const snapshot = await transaction.get(candidate.ref);
        const request = snapshotData(snapshot);
        if (!request || request.updatedAt >= cutoff) {
          return 0;
        }
        const mappingRef = IDEMPOTENCY_HASH_PATTERN.test(request.idempotencyHash || '')
          ? this.idempotencyRef(request.idempotencyHash)
          : null;
        const mappingSnapshot = mappingRef ? await transaction.get(mappingRef) : null;
        transaction.delete(candidate.ref);
        if (mappingRef && mappingSnapshot?.exists) {
          transaction.delete(mappingRef);
          return 2;
        }
        return 1;
      });
    }
    return removed;
  }
}
