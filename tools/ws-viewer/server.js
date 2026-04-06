const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

const WS_PORT = Number(process.env.WS_PORT || 9001);
const HTTP_PORT = Number(process.env.HTTP_PORT || 8080);

const latestByCamera = new Map();
const cameraSessions = new Map();
const viewers = new Set();

function sendJson(ws, payload) {
  if (ws.readyState === ws.OPEN) {
    ws.send(JSON.stringify(payload));
  }
}

function broadcastJson(payload) {
  const body = JSON.stringify(payload);
  for (const client of viewers) {
    if (client.readyState === client.OPEN) {
      client.send(body);
    }
  }
}

function broadcastFrame(frame) {
  const header = {
    type: 'viewer_frame_meta',
    camera_id: frame.cameraId,
    user_id: frame.userId,
    len: frame.len,
    ts: frame.ts,
    mime: 'image/jpeg'
  };

  const headerText = JSON.stringify(header);
  for (const client of viewers) {
    if (client.readyState !== client.OPEN) {
      continue;
    }
    client.send(headerText);
    client.send(frame.jpegBuffer);
  }
}

function createWsRelayServer() {
  const httpServer = http.createServer();
  const wssCam = new WebSocketServer({ noServer: true });
  const wssViewer = new WebSocketServer({ noServer: true });

  httpServer.on('upgrade', (request, socket, head) => {
    const url = new URL(request.url, `http://${request.headers.host}`);
    if (url.pathname === '/ws') {
      wssCam.handleUpgrade(request, socket, head, (ws) => {
        wssCam.emit('connection', ws, request);
      });
      return;
    }
    if (url.pathname === '/viewer') {
      wssViewer.handleUpgrade(request, socket, head, (ws) => {
        wssViewer.emit('connection', ws, request);
      });
      return;
    }
    socket.destroy();
  });

  wssCam.on('connection', (ws, request) => {
    const from = request.socket.remoteAddress || 'unknown';
    console.log(`[cam] connected from ${from}`);
    sendJson(ws, { type: 'relay_hello', role: 'camera' });

    cameraSessions.set(ws, {
      waitingBinary: false,
      meta: null,
      connectedAt: Date.now()
    });

    ws.on('message', (data, isBinary) => {
      const session = cameraSessions.get(ws);
      if (!session) {
        return;
      }

      if (!isBinary) {
        let meta;
        try {
          meta = JSON.parse(data.toString('utf8'));
        } catch (err) {
          console.warn('[cam] bad json:', err.message);
          return;
        }

        if (meta.type !== 'frame_meta') {
          return;
        }

        if (!Number.isInteger(meta.camera_id) || !Number.isInteger(meta.len)) {
          console.warn('[cam] invalid meta fields');
          return;
        }

        session.meta = meta;
        session.waitingBinary = true;
        return;
      }

      if (!session.waitingBinary || !session.meta) {
        return;
      }

      const jpegBuffer = Buffer.from(data);
      if (jpegBuffer.length !== session.meta.len) {
        console.warn(
          `[cam] length mismatch camera_id=${session.meta.camera_id} expected=${session.meta.len} got=${jpegBuffer.length}`
        );
        session.waitingBinary = false;
        session.meta = null;
        return;
      }

      const frame = {
        cameraId: session.meta.camera_id,
        userId: session.meta.user_id,
        len: session.meta.len,
        ts: session.meta.ts,
        jpegBuffer
      };

      latestByCamera.set(frame.cameraId, {
        ...frame,
        jpegBase64: jpegBuffer.toString('base64')
      });

      broadcastFrame(frame);
      session.waitingBinary = false;
      session.meta = null;
    });

    ws.on('close', () => {
      cameraSessions.delete(ws);
      console.log(`[cam] disconnected ${from}`);
    });

    ws.on('error', (err) => {
      cameraSessions.delete(ws);
      console.warn('[cam] error:', err.message);
    });
  });

  wssViewer.on('connection', (ws, request) => {
    const from = request.socket.remoteAddress || 'unknown';
    viewers.add(ws);
    console.log(`[viewer] connected from ${from}`);

    sendJson(ws, {
      type: 'viewer_hello',
      message: 'connected',
      cameras: Array.from(latestByCamera.keys())
    });

    for (const frame of latestByCamera.values()) {
      sendJson(ws, {
        type: 'viewer_snapshot',
        camera_id: frame.cameraId,
        user_id: frame.userId,
        len: frame.len,
        ts: frame.ts,
        mime: 'image/jpeg',
        jpeg_base64: frame.jpegBase64
      });
    }

    ws.on('close', () => {
      viewers.delete(ws);
      console.log(`[viewer] disconnected ${from}`);
    });

    ws.on('error', () => {
      viewers.delete(ws);
    });
  });

  httpServer.listen(WS_PORT, () => {
    console.log(`[relay] websocket listening ws://0.0.0.0:${WS_PORT}`);
    console.log(`[relay] camera endpoint: ws://<host>:${WS_PORT}/ws`);
    console.log(`[relay] browser endpoint: ws://<host>:${WS_PORT}/viewer`);
  });
}

function createHttpViewerServer() {
  const publicDir = path.join(__dirname, 'public');

  const server = http.createServer((req, res) => {
    const reqPath = req.url === '/' ? '/index.html' : req.url;
    const fsPath = path.normalize(path.join(publicDir, reqPath));
    if (!fsPath.startsWith(publicDir)) {
      res.writeHead(403);
      res.end('forbidden');
      return;
    }

    fs.readFile(fsPath, (err, data) => {
      if (err) {
        res.writeHead(404);
        res.end('not found');
        return;
      }

      const ext = path.extname(fsPath);
      const contentType =
        ext === '.html'
          ? 'text/html; charset=utf-8'
          : ext === '.js'
          ? 'application/javascript; charset=utf-8'
          : ext === '.css'
          ? 'text/css; charset=utf-8'
          : 'application/octet-stream';

      res.writeHead(200, { 'Content-Type': contentType });
      res.end(data);
    });
  });

  server.listen(HTTP_PORT, () => {
    console.log(`[viewer] open http://127.0.0.1:${HTTP_PORT}`);
  });
}

createWsRelayServer();
createHttpViewerServer();
