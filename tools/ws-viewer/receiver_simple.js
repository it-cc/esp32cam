const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

const WS_PORT = Number(process.env.WS_PORT || 9001);
const HTTP_PORT = Number(process.env.HTTP_PORT || 8080);

const captureDir = path.join(__dirname, 'captures');
fs.mkdirSync(captureDir, { recursive: true });

let latestFrame = null;

const pageHtml = `<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32-CAM Simple Receiver</title>
  <style>
    body { margin: 16px; font-family: Arial, sans-serif; background: #111; color: #eee; }
    h1 { margin: 0 0 10px 0; font-size: 20px; }
    .meta { font-size: 13px; color: #bbb; margin-bottom: 10px; }
    img { width: 100%; max-width: 860px; border: 1px solid #444; border-radius: 8px; }
  </style>
</head>
<body>
  <h1>ESP32-CAM Live Snapshot</h1>
  <div class="meta" id="meta">waiting frame...</div>
  <img id="img" src="/frame.jpg" alt="camera frame" />
  <script>
    const img = document.getElementById('img');
    const meta = document.getElementById('meta');

    async function refresh() {
      try {
        const statusResp = await fetch('/status');
        if (statusResp.ok) {
          const s = await statusResp.json();
          if (s.has_frame) {
            meta.textContent = 'camera_id=' + s.camera_id + ' user_id=' + s.user_id +
              ' len=' + s.len + ' ts=' + s.ts;
            img.src = '/frame.jpg?t=' + Date.now();
          } else {
            meta.textContent = 'waiting frame...';
          }
        }
      } catch (err) {
        meta.textContent = 'status error: ' + err.message;
      }
    }

    setInterval(refresh, 700);
    refresh();
  </script>
</body>
</html>`;

function handleCameraConnection(ws, remote) {
  console.log(`[cam] connected from ${remote}`);

  let pendingMeta = null;

  ws.on('message', (data, isBinary) => {
    if (!isBinary) {
      let msg;
      try {
        msg = JSON.parse(data.toString('utf8'));
      } catch (err) {
        console.warn('[cam] bad json:', err.message);
        return;
      }

      if (msg.type !== 'frame_meta') {
        return;
      }

      if (!Number.isInteger(msg.len) || !Number.isInteger(msg.camera_id)) {
        console.warn('[cam] invalid frame_meta fields');
        return;
      }

      pendingMeta = msg;
      return;
    }

    if (!pendingMeta) {
      return;
    }

    const jpeg = Buffer.from(data);
    if (jpeg.length !== pendingMeta.len) {
      console.warn(`[cam] len mismatch expected=${pendingMeta.len} got=${jpeg.length}`);
      pendingMeta = null;
      return;
    }

    latestFrame = {
      cameraId: pendingMeta.camera_id,
      userId: pendingMeta.user_id,
      ts: pendingMeta.ts,
      len: pendingMeta.len,
      jpeg,
    };

    // Keep one latest frame for quick browser preview.
    fs.writeFile(path.join(captureDir, 'latest.jpg'), jpeg, (err) => {
      if (err) {
        console.warn('[cam] save latest.jpg failed:', err.message);
      }
    });

    pendingMeta = null;
  });

  ws.on('close', () => console.log(`[cam] disconnected ${remote}`));
  ws.on('error', (err) => console.warn('[cam] error:', err.message));
}

function startWsServer() {
  const upgradeServer = http.createServer();
  const camWss = new WebSocketServer({ noServer: true });

  upgradeServer.on('upgrade', (req, socket, head) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (url.pathname !== '/ws') {
      socket.destroy();
      return;
    }

    camWss.handleUpgrade(req, socket, head, (ws) => {
      camWss.emit('connection', ws, req);
    });
  });

  camWss.on('connection', (ws, req) => {
    const remote = req.socket.remoteAddress || 'unknown';
    handleCameraConnection(ws, remote);
  });

  upgradeServer.listen(WS_PORT, () => {
    console.log(`[relay] camera endpoint ws://0.0.0.0:${WS_PORT}/ws`);
  });
}

function startHttpServer() {
  const server = http.createServer((req, res) => {
    if (req.url === '/' || req.url === '/index.html') {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(pageHtml);
      return;
    }

    if (req.url && req.url.startsWith('/frame.jpg')) {
      if (!latestFrame) {
        res.writeHead(503, { 'Content-Type': 'text/plain; charset=utf-8' });
        res.end('waiting frame');
        return;
      }

      res.writeHead(200, {
        'Content-Type': 'image/jpeg',
        'Cache-Control': 'no-store, no-cache, must-revalidate',
      });
      res.end(latestFrame.jpeg);
      return;
    }

    if (req.url === '/status') {
      const payload = latestFrame
        ? {
            has_frame: true,
            camera_id: latestFrame.cameraId,
            user_id: latestFrame.userId,
            len: latestFrame.len,
            ts: latestFrame.ts,
          }
        : { has_frame: false };

      res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
      res.end(JSON.stringify(payload));
      return;
    }

    res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
    res.end('not found');
  });

  server.listen(HTTP_PORT, () => {
    console.log(`[viewer] open http://127.0.0.1:${HTTP_PORT}`);
    console.log(`[viewer] latest frame file ${path.join(captureDir, 'latest.jpg')}`);
  });
}

startWsServer();
startHttpServer();
