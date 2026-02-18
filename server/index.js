/**
 * G500 RC Car — Main Server
 *
 * Responsibilities:
 *  1. Serve the UI static files
 *  2. Accept WebSocket connections from the phone (drive commands)
 *  3. Forward drive commands to Arduino Nano over USB serial
 *  4. Proxy the camera MJPEG stream from the camera service
 */

const express = require('express');
const http = require('http');
const { WebSocketServer } = require('ws');
const path = require('path');
const { SerialPort } = require('serialport');

// ─── Config ────────────────────────────────────────────────────────────────
const PORT = process.env.PORT || 3000;
const SERIAL_PATH = process.env.SERIAL_PATH || null; // e.g. /dev/ttyUSB0
const SERIAL_BAUD = parseInt(process.env.SERIAL_BAUD || '115200', 10);
const CAMERA_URL = process.env.CAMERA_URL || 'http://localhost:8080/stream';
const UI_DIR = path.join(__dirname, '..', 'ui');

// ─── Express App ────────────────────────────────────────────────────────────
const app = express();
const server = http.createServer(app);

// Serve UI static files
app.use(express.static(UI_DIR));

// Health check
app.get('/health', (_req, res) => res.json({ status: 'ok' }));

// Camera stream proxy — avoids CORS issues on the phone
app.get('/stream', (req, res) => {
  const cameraReq = http.get(CAMERA_URL, (cameraRes) => {
    res.writeHead(cameraRes.statusCode, cameraRes.headers);
    cameraRes.pipe(res);
  });
  cameraReq.on('error', (err) => {
    console.error('[camera] proxy error:', err.message);
    if (!res.headersSent) {
      res.status(502).json({ error: 'Camera unavailable', detail: err.message });
    }
  });
  req.on('close', () => cameraReq.destroy());
});

// ─── Serial Port ────────────────────────────────────────────────────────────
let serial = null;

async function detectSerialPort() {
  if (SERIAL_PATH) return SERIAL_PATH;
  try {
    const ports = await SerialPort.list();
    const candidate = ports.find(
      (p) =>
        p.path.includes('ttyUSB') ||
        p.path.includes('ttyACM') ||
        p.path.includes('usbmodem') // macOS
    );
    return candidate ? candidate.path : null;
  } catch {
    return null;
  }
}

async function openSerial() {
  const portPath = await detectSerialPort();
  if (!portPath) {
    console.warn('[serial] No Arduino detected. Running without serial (mock mode).');
    return;
  }

  serial = new SerialPort({ path: portPath, baudRate: SERIAL_BAUD });

  serial.on('open', () => console.log(`[serial] Opened ${portPath} @ ${SERIAL_BAUD} baud`));
  serial.on('error', (err) => {
    console.error('[serial] Error:', err.message);
    serial = null;
    // Retry after 3 seconds
    setTimeout(openSerial, 3000);
  });
  serial.on('close', () => {
    console.warn('[serial] Port closed. Reconnecting in 3s...');
    serial = null;
    setTimeout(openSerial, 3000);
  });
}

function sendCommand(throttle, steering) {
  if (!serial || !serial.isOpen) return;
  const packet = JSON.stringify({ t: throttle, s: steering }) + '\n';
  serial.write(packet, (err) => {
    if (err) console.error('[serial] Write error:', err.message);
  });
}

// ─── WebSocket Server ───────────────────────────────────────────────────────
const wss = new WebSocketServer({ server });

// Track the last command time for server-side logging
let connectedClients = 0;

wss.on('connection', (ws, req) => {
  connectedClients++;
  const ip = req.socket.remoteAddress;
  console.log(`[ws] Client connected: ${ip} (total: ${connectedClients})`);

  ws.on('message', (raw) => {
    let cmd;
    try {
      cmd = JSON.parse(raw.toString());
    } catch {
      console.warn('[ws] Invalid JSON from client:', raw.toString());
      return;
    }

    const throttle = clamp(parseFloat(cmd.throttle ?? 0), -1, 1);
    const steering = clamp(parseFloat(cmd.steering ?? 0), -1, 1);

    sendCommand(throttle, steering);
  });

  ws.on('close', () => {
    connectedClients--;
    console.log(`[ws] Client disconnected: ${ip} (total: ${connectedClients})`);
    // Safety: stop the car when the last client disconnects
    if (connectedClients === 0) {
      sendCommand(0, 0);
    }
  });

  ws.on('error', (err) => console.error('[ws] Socket error:', err.message));

  // Send current status to new client
  ws.send(JSON.stringify({ type: 'status', serial: !!serial, camera: CAMERA_URL }));
});

// ─── Helpers ────────────────────────────────────────────────────────────────
function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

// ─── Start ──────────────────────────────────────────────────────────────────
(async () => {
  await openSerial();
  server.listen(PORT, '0.0.0.0', () => {
    console.log(`[server] G500 running on http://0.0.0.0:${PORT}`);
    console.log(`[server] UI:     http://0.0.0.0:${PORT}/`);
    console.log(`[server] Stream: http://0.0.0.0:${PORT}/stream`);
    console.log(`[server] WS:     ws://0.0.0.0:${PORT}`);
  });
})();
