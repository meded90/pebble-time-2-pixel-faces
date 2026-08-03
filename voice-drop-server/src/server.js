import { timingSafeEqual } from 'node:crypto';
import { execFile } from 'node:child_process';
import { mkdir, readFile, rename, writeFile } from 'node:fs/promises';
import http from 'node:http';
import { join } from 'node:path';
import { promisify } from 'node:util';
import { buildSpeexOgg } from './ogg-speex.js';
import { MAX_PAYLOAD_SIZE, parseVoiceDrop } from './container.js';

const execFileAsync = promisify(execFile);
const port = Number(process.env.PORT || 8787);
const dataDir = process.env.DATA_DIR || '/data';
const botToken = required('TELEGRAM_BOT_TOKEN');
const chatId = required('TELEGRAM_CHAT_ID');
const deviceTokens = new Set(required('VOICE_DROP_DEVICE_TOKENS').split(',').map((v) => v.trim()));

await mkdir(join(dataDir, 'recordings'), { recursive: true });
await mkdir(join(dataDir, 'delivered'), { recursive: true });

http.createServer(async (request, response) => {
  try {
    if (request.method === 'GET' && request.url === '/healthz') {
      return json(response, 200, { ok: true });
    }
    if (request.method !== 'POST' || request.url !== '/v1/recordings') {
      return json(response, 404, { error: 'not_found' });
    }
    if (request.headers['content-type'] !== 'application/x-voice-drop') {
      return json(response, 415, { error: 'unsupported_media_type' });
    }
    if (!authorized(request.headers.authorization)) {
      return json(response, 401, { error: 'unauthorized' });
    }
    const container = await readBody(request, MAX_PAYLOAD_SIZE + 40);
    const recording = parseVoiceDrop(container);
    const deliveryMarker = join(dataDir, 'delivered', `${recording.id}.json`);
    if (await exists(deliveryMarker)) {
      return json(response, 200, { ok: true, duplicate: true, id: recording.id });
    }

    const stem = join(dataDir, 'recordings', String(recording.id));
    await atomicWrite(`${stem}.vdrop`, container);
    await atomicWrite(`${stem}.spx`, buildSpeexOgg(recording));
    await transcode(`${stem}.spx`, `${stem}.ogg`);
    await sendTelegramVoice(`${stem}.ogg`, recording);
    await atomicWrite(deliveryMarker, Buffer.from(JSON.stringify({
      id: recording.id,
      deliveredAt: new Date().toISOString(),
    })));
    return json(response, 201, { ok: true, id: recording.id });
  } catch (error) {
    console.error(error);
    return json(response, error.statusCode || 500, { error: error.message });
  }
}).listen(port, '0.0.0.0', () => {
  console.log(`Voice Drop server listening on ${port}`);
});

function required(name) {
  const value = process.env[name];
  if (!value) throw new Error(`${name} is required`);
  return value;
}

function authorized(header) {
  if (!header?.startsWith('Bearer ')) return false;
  const candidate = Buffer.from(header.slice(7));
  return [...deviceTokens].some((token) => {
    const expected = Buffer.from(token);
    return candidate.length === expected.length && timingSafeEqual(candidate, expected);
  });
}

async function readBody(request, limit) {
  const chunks = [];
  let length = 0;
  for await (const chunk of request) {
    length += chunk.length;
    if (length > limit) throw Object.assign(new Error('payload_too_large'), { statusCode: 413 });
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}

async function atomicWrite(path, bytes) {
  const temporary = `${path}.${process.pid}.tmp`;
  await writeFile(temporary, bytes, { mode: 0o600 });
  await rename(temporary, path);
}

async function transcode(input, output) {
  await execFileAsync('ffmpeg', [
    '-hide_banner', '-loglevel', 'error', '-y', '-i', input,
    '-c:a', 'libopus', '-b:a', '24k', '-application', 'voip', output,
  ]);
}

async function sendTelegramVoice(path, recording) {
  const voice = await readFile(path);
  const form = new FormData();
  form.set('chat_id', chatId);
  form.set('voice', new Blob([voice], { type: 'audio/ogg' }), `voice-${recording.id}.ogg`);
  form.set('duration', String(Math.round(recording.durationMs / 1000)));
  form.set('caption', `${new Date(recording.startedAt * 1000).toISOString()} · ${
    Math.round(recording.durationMs / 1000)} sec`);
  const result = await fetch(`https://api.telegram.org/bot${botToken}/sendVoice`, {
    method: 'POST',
    body: form,
  });
  const body = await result.json();
  if (!result.ok || body.ok !== true) throw new Error(`Telegram sendVoice failed: ${body.description || result.status}`);
}

async function exists(path) {
  try {
    await readFile(path);
    return true;
  } catch (error) {
    if (error.code === 'ENOENT') return false;
    throw error;
  }
}

function json(response, status, body) {
  response.writeHead(status, { 'content-type': 'application/json; charset=utf-8' });
  response.end(JSON.stringify(body));
}
