import { http } from '@google-cloud/functions-framework';
import { createApp } from './app.js';
import { loadConfig } from './config.js';
import { FirestoreRequestStore } from './firestore-store.js';
import { WorkspaceAgentClient } from './workspace-agent-client.js';

let applicationPromise;

async function createFunctionApplication() {
  const config = loadConfig();
  const store = new FirestoreRequestStore({
    capabilityPepper: config.capabilityPepper,
    collectionPrefix: config.firestoreCollectionPrefix,
    retentionMs: config.retentionMs,
  });
  await store.init();

  const agentClient = new WorkspaceAgentClient({
    triggerId: config.workspaceAgentTriggerId,
    accessToken: config.workspaceAgentAccessToken,
    timeoutMs: config.agentTimeoutMs,
    maxAttempts: config.agentMaxAttempts,
  });
  return createApp({ config, store, agentClient });
}

function getFunctionApplication() {
  if (!applicationPromise) {
    applicationPromise = createFunctionApplication().catch((error) => {
      // A transient cold-start failure (for example, a just-created Firestore
      // database) must not poison all later warm invocations.
      applicationPromise = undefined;
      throw error;
    });
  }
  return applicationPromise;
}

export async function wristAgentBridge(request, response) {
  try {
    const app = await getFunctionApplication();
    return app(request, response);
  } catch (error) {
    console.error(JSON.stringify({
      level: 'error',
      event: 'function_initialization_failed',
      message: error?.message || 'unknown error',
    }));
    response.status(503).json({
      error: { code: 'INITIALIZING', message: 'Service temporarily unavailable' },
    });
    return undefined;
  }
}

// Cloud Functions Gen 2 discovers this registered HTTP entry point through
// package.json#main and --entry-point=wristAgentBridge.
http('wristAgentBridge', wristAgentBridge);
