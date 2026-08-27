import { createServer } from 'node:http';
import { createApp } from './app.js';
import { loadConfig } from './config.js';
import { RequestStore } from './store.js';
import { WorkspaceAgentClient } from './workspace-agent-client.js';

const config = loadConfig();
const store = new RequestStore({
  directory: config.dataDir,
  capabilityPepper: config.capabilityPepper,
});
await store.init();

const agentClient = new WorkspaceAgentClient({
  triggerId: config.workspaceAgentTriggerId,
  accessToken: config.workspaceAgentAccessToken,
  timeoutMs: config.agentTimeoutMs,
  maxAttempts: config.agentMaxAttempts,
});

const app = createApp({ config, store, agentClient });
const server = createServer(app);

server.listen(config.port, '0.0.0.0', () => {
  console.log(JSON.stringify({
    level: 'info',
    event: 'server_started',
    port: config.port,
    dataDir: config.dataDir,
  }));
});

const cleanupTimer = setInterval(async () => {
  try {
    const removed = await store.cleanup(config.retentionMs);
    if (removed > 0) {
      console.log(JSON.stringify({ level: 'info', event: 'expired_records_removed', removed }));
    }
  } catch (error) {
    console.error(JSON.stringify({
      level: 'error',
      event: 'cleanup_failed',
      message: error?.message || 'unknown error',
    }));
  }
}, 60 * 60 * 1000);
cleanupTimer.unref();

function shutdown(signal) {
  console.log(JSON.stringify({ level: 'info', event: 'shutdown', signal }));
  clearInterval(cleanupTimer);
  server.close((error) => {
    process.exitCode = error ? 1 : 0;
  });
  setTimeout(() => {
    process.exitCode = 1;
    server.closeAllConnections();
  }, 10000).unref();
}

process.once('SIGINT', () => shutdown('SIGINT'));
process.once('SIGTERM', () => shutdown('SIGTERM'));
