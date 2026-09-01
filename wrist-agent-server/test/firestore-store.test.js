import assert from 'node:assert/strict';
import test from 'node:test';
import { FirestoreRequestStore } from '../src/firestore-store.js';
import { sha256 } from '../src/store.js';

class FakeSnapshot {
  constructor(ref, value) {
    this.ref = ref;
    this.exists = value !== undefined;
    this.value = value;
  }

  data() {
    return structuredClone(this.value);
  }
}

class FakeDocumentReference {
  constructor(firestore, collection, id) {
    this.firestore = firestore;
    this.collection = collection;
    this.id = id;
  }

  get path() {
    return `${this.collection}/${this.id}`;
  }

  async get() {
    return this.firestore.snapshot(this);
  }
}

class FakeQuery {
  constructor(firestore, collection, filters = [], maximum = null) {
    this.firestore = firestore;
    this.collection = collection;
    this.filters = filters;
    this.maximum = maximum;
  }

  where(field, operator, value) {
    return new FakeQuery(this.firestore, this.collection, [
      ...this.filters,
      { field, operator, value },
    ], this.maximum);
  }

  limit(maximum) {
    return new FakeQuery(this.firestore, this.collection, this.filters, maximum);
  }

  async get() {
    const prefix = `${this.collection}/`;
    const docs = [...this.firestore.data.entries()]
      .filter(([path, value]) => path.startsWith(prefix) &&
        this.filters.every(({ field, operator, value: expected }) => {
          if (operator === '<') {
            return value[field] < expected;
          }
          throw new Error(`unsupported fake query operator: ${operator}`);
        }))
      .sort(([left], [right]) => left.localeCompare(right))
      .slice(0, this.maximum || undefined)
      .map(([path, value]) => {
        const id = path.slice(prefix.length);
        const ref = new FakeDocumentReference(this.firestore, this.collection, id);
        return new FakeSnapshot(ref, value);
      });
    return { docs };
  }
}

class FakeCollectionReference extends FakeQuery {
  constructor(firestore, collection) {
    super(firestore, collection);
  }

  doc(id) {
    return new FakeDocumentReference(this.firestore, this.collection, id);
  }
}

class FakeTransaction {
  constructor(firestore) {
    this.firestore = firestore;
    this.writes = new Map();
  }

  async get(ref) {
    return this.firestore.snapshot(ref);
  }

  set(ref, value) {
    this.writes.set(ref.path, { type: 'set', value: structuredClone(value) });
  }

  delete(ref) {
    this.writes.set(ref.path, { type: 'delete' });
  }

  commit() {
    for (const [path, write] of this.writes) {
      if (write.type === 'delete') {
        this.firestore.data.delete(path);
      } else {
        this.firestore.data.set(path, write.value);
      }
    }
  }
}

class FakeFirestore {
  constructor() {
    this.data = new Map();
  }

  collection(name) {
    return new FakeCollectionReference(this, name);
  }

  snapshot(ref) {
    return new FakeSnapshot(ref, this.data.get(ref.path));
  }

  async runTransaction(callback) {
    const transaction = new FakeTransaction(this);
    const result = await callback(transaction);
    transaction.commit();
    return result;
  }
}

function request(timestamp) {
  return {
    schemaVersion: 1,
    requestId: 'wrq_abcdefghijkl',
    principalHash: sha256('device-token'),
    status: 'trigger_pending',
    triggerLeaseId: null,
    triggerLeaseExpiresAt: 0,
    expiresAt: timestamp + 5000,
    createdAt: timestamp,
    updatedAt: timestamp,
  };
}

test('Firestore store atomically preserves idempotency, leases, and cleanup metadata', async () => {
  let clock = 1000;
  const firestore = new FakeFirestore();
  const store = new FirestoreRequestStore({
    firestore,
    capabilityPepper: 'a-long-independent-capability-pepper-for-tests',
    collectionPrefix: 'wrist_agent',
    retentionMs: 3000,
    now: () => clock,
  });
  await store.init();

  const idempotency = {
    hash: sha256('principal:request-key'),
    bodyHash: sha256('request body'),
  };
  const created = await store.create(request(clock), idempotency);
  assert.equal(created.created, true);
  assert.equal(created.request.idempotencyHash, idempotency.hash);
  assert(created.request.purgeAt instanceof Date);

  const duplicate = await store.create(request(clock), idempotency);
  assert.equal(duplicate.created, false);
  assert.equal(duplicate.request.requestId, created.request.requestId);

  const claim = await store.claimTrigger(created.request.requestId, 2000);
  assert.equal(claim.claimed, true);
  assert.equal(claim.request.status, 'triggering');
  assert.ok(claim.leaseId);
  const secondClaim = await store.claimTrigger(created.request.requestId, 2000);
  assert.equal(secondClaim.claimed, false);
  assert.equal(secondClaim.request.triggerLeaseId, claim.leaseId);

  const completed = await store.update(created.request.requestId, (current) => {
    current.status = 'completed';
    current.triggerLeaseId = null;
    current.triggerLeaseExpiresAt = 0;
    return current;
  });
  assert.equal(completed.status, 'completed');
  assert(completed.purgeAt instanceof Date);

  clock += 4000;
  assert.equal(await store.cleanup(3000), 2);
  assert.equal(await store.get(created.request.requestId), null);
  assert.equal(await store.getByIdempotency(idempotency.hash), null);
});
