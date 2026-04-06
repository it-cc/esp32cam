#include "esp32cam_app.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <Wire.h>

#ifndef APP_WIFI_USE_STATIC_IP
#define APP_WIFI_USE_STATIC_IP 0
#endif

#ifndef APP_WIFI_STATIC_IP
#define APP_WIFI_STATIC_IP "192.168.1.123"
#endif

#ifndef APP_WIFI_STATIC_GATEWAY
#define APP_WIFI_STATIC_GATEWAY "192.168.1.1"
#endif

#ifndef APP_WIFI_STATIC_SUBNET
#define APP_WIFI_STATIC_SUBNET "255.255.255.0"
#endif

#ifndef APP_WIFI_STATIC_DNS1
#define APP_WIFI_STATIC_DNS1 "8.8.8.8"
#endif

#ifndef APP_WIFI_STATIC_DNS2
#define APP_WIFI_STATIC_DNS2 "1.1.1.1"
#endif

Esp32CamApp *Esp32CamApp::instance_ = nullptr;

namespace
{
const char kPreviewHtml[] =
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32-CAM Preview</title>"
    "<style>body{font-family:Arial,sans-serif;margin:12px;background:#111;"
    "color:#eee;}"
    "h1{font-size:18px;margin:0 0 "
    "10px;}img{width:100%;max-width:800px;border:1px solid "
    "#444;border-radius:8px;}"
    "p{font-size:13px;color:#bbb;}</style></head><body>"
    "<h1>ESP32-CAM Preview</h1>"
    "<img id='cam' src='/snapshot.jpg' alt='snapshot'>"
    "<p id='s'>loading...</p>"
    "<script>"
    "const i=document.getElementById('cam');const "
    "s=document.getElementById('s');"
    "let ok=0,fail=0;"
    "setInterval(()=>{const t=Date.now();i.src='/snapshot.jpg?t='+t;},700);"
    "i.onload=()=>{ok++;s.textContent='ok='+ok+' fail='+fail+' '+new "
    "Date().toLocaleTimeString();};"
    "i.onerror=()=>{fail++;s.textContent='ok='+ok+' fail='+fail+' '+new "
    "Date().toLocaleTimeString();};"
    "</script></body></html>";
}

uint8_t Esp32CamApp::crc8(const uint8_t *data, size_t len)
{
  if (data == nullptr)
  {
    return 0;
  }

  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x80)
      {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

bool Esp32CamApp::decodeFrame(const uint8_t *data, size_t len, uint8_t &msgType,
                              uint8_t &seq, const uint8_t *&payload,
                              uint8_t &payloadLen, uint8_t &decodeErr)
{
  decodeErr = kErrOk;
  msgType = 0;
  seq = 0;
  payload = nullptr;
  payloadLen = 0;

  if (data == nullptr || len < kMinFrameLen || len > kMaxFrameLen)
  {
    decodeErr = kErrBadLen;
    return false;
  }

  if (data[0] != kProtocolVersion)
  {
    decodeErr = kErrBadLen;
    return false;
  }

  const uint8_t parsedPayloadLen = data[3];
  const size_t expectedLen = (size_t)kMinFrameLen + parsedPayloadLen;
  if (parsedPayloadLen > kMaxPayloadLen || expectedLen != len)
  {
    decodeErr = kErrBadLen;
    return false;
  }

  const uint8_t expectedCrc = crc8(data, expectedLen - 1);
  if (data[expectedLen - 1] != expectedCrc)
  {
    decodeErr = kErrBadCrc;
    return false;
  }

  msgType = data[1];
  seq = data[2];
  payloadLen = parsedPayloadLen;
  payload = &data[4];
  return true;
}

bool Esp32CamApp::encodeResponse(uint8_t msgType, uint8_t seq,
                                 const uint8_t *payload, uint8_t payloadLen)
{
  if (payloadLen > kMaxPayloadLen)
  {
    return false;
  }

  if (payloadLen > 0 && payload == nullptr)
  {
    return false;
  }

  pendingResponse_[0] = kProtocolVersion;
  pendingResponse_[1] = msgType;
  pendingResponse_[2] = seq;
  pendingResponse_[3] = payloadLen;

  for (uint8_t i = 0; i < payloadLen; ++i)
  {
    pendingResponse_[4 + i] = payload[i];
  }

  const uint8_t totalLen = (uint8_t)(kMinFrameLen + payloadLen);
  pendingResponse_[totalLen - 1] = crc8(pendingResponse_, totalLen - 1);
  pendingResponseLen_ = totalLen;
  return true;
}

bool Esp32CamApp::buildAck(uint8_t msgType, uint8_t seq, uint8_t errCode)
{
  uint8_t payload[1] = {errCode};
  return encodeResponse(msgType, seq, payload, 1);
}

bool Esp32CamApp::buildStatusResponse(uint8_t seq)
{
  uint8_t payload[5] = {
      status_,
      lastError_,
      lastSeq_,
      (uint8_t)(frameCounter_ & 0xFF),
      (uint8_t)((frameCounter_ >> 8) & 0xFF),
  };
  return encodeResponse(kMsgGetStatus, seq, payload, 5);
}

bool Esp32CamApp::handleProvisionPayload(const uint8_t *payload,
                                         uint8_t payloadLen)
{
  if (payload == nullptr || payloadLen < 1)
  {
    lastError_ = kErrBadLen;
    return false;
  }

  uint8_t marker = payload[0];
  if (marker == kFrameStart)
  {
    i2cBuffer_ = "";
    return true;
  }

  if (marker == kFrameChunk)
  {
    for (uint8_t i = 1; i < payloadLen; ++i)
    {
      i2cBuffer_ += (char)payload[i];
    }
    return true;
  }

  if (marker == kFrameEnd)
  {
    configPending_ = true;
    return true;
  }

  lastError_ = kErrUnsupportedCmd;
  return false;
}

bool Esp32CamApp::handleWifiConfigPayload(const uint8_t *payload,
                                          uint8_t payloadLen)
{
  if (payload == nullptr || payloadLen < 2)
  {
    lastError_ = kErrBadLen;
    return false;
  }

  const uint8_t ssidLen = payload[0];
  const uint8_t pwdLen = payload[1];
  const uint8_t expectedLen = (uint8_t)(2 + ssidLen + pwdLen);
  if (ssidLen == 0 || expectedLen != payloadLen)
  {
    lastError_ = kErrBadLen;
    return false;
  }

  String ssid;
  String password;
  for (uint8_t i = 0; i < ssidLen; ++i)
  {
    ssid += (char)payload[2 + i];
  }
  for (uint8_t i = 0; i < pwdLen; ++i)
  {
    password += (char)payload[2 + ssidLen + i];
  }

  cfg_.ssid = ssid;
  cfg_.password = password;
  cfg_.valid = cfg_.ssid.length() > 0 && cfg_.wsUrl.length() > 0;
  pendingNetConfigApply_ = cfg_.valid;
  status_ = cfg_.valid ? kStatusConfigReady : kStatusIdle;
  return true;
}

bool Esp32CamApp::handleWsConfigPayload(const uint8_t *payload,
                                        uint8_t payloadLen)
{
  if (payload == nullptr || payloadLen < 5)
  {
    lastError_ = kErrBadLen;
    return false;
  }

  const uint8_t hostLen = payload[0];
  const uint8_t pathLen = payload[1];
  const uint16_t port =
      (uint16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8));
  const bool useTls = payload[4] != 0;

  const uint8_t expectedLen = (uint8_t)(5 + hostLen + pathLen);
  if (hostLen == 0 || pathLen == 0 || port == 0 || expectedLen != payloadLen)
  {
    lastError_ = kErrBadLen;
    return false;
  }

  String host;
  String path;
  for (uint8_t i = 0; i < hostLen; ++i)
  {
    host += (char)payload[5 + i];
  }
  for (uint8_t i = 0; i < pathLen; ++i)
  {
    path += (char)payload[5 + hostLen + i];
  }

  if (!path.startsWith("/"))
  {
    path = "/" + path;
  }

  cfg_.wsUrl = String(useTls ? "wss://" : "ws://") + host + ":" +
               String((unsigned int)port) + path;
  cfg_.valid = cfg_.ssid.length() > 0 && cfg_.wsUrl.length() > 0;
  pendingNetConfigApply_ = cfg_.valid;
  status_ = cfg_.valid ? kStatusConfigReady : kStatusIdle;
  return true;
}

bool Esp32CamApp::handleBinaryRequest(const uint8_t *data, size_t len)
{
  uint8_t msgType = 0;
  uint8_t seq = 0;
  const uint8_t *payload = nullptr;
  uint8_t payloadLen = 0;
  uint8_t decodeErr = kErrOk;

  if (!decodeFrame(data, len, msgType, seq, payload, payloadLen, decodeErr))
  {
    // Decode failed before we can trust msgType/seq; master will retry on
    // short/CRC errors.
    lastError_ = decodeErr;
    pendingResponseLen_ = 0;
    return false;
  }

  lastSeq_ = seq;
  ++frameCounter_;

  bool ok = false;
  switch (msgType)
  {
    case kMsgPing:
      lastError_ = kErrOk;
      ok = buildAck(msgType, seq, kErrOk);
      break;

    case kMsgGetStatus:
      lastError_ = kErrOk;
      ok = buildStatusResponse(seq);
      break;

    case kMsgTriggerCapture:
      // Current app streams continuously; this command is accepted as no-op.
      lastError_ = kErrOk;
      ok = buildAck(msgType, seq, kErrOk);
      break;

    case kMsgRebootNode:
      lastError_ = kErrOk;
      pendingReboot_ = true;
      ok = buildAck(msgType, seq, kErrOk);
      break;

    case kMsgProvisionFrame:
      if (handleProvisionPayload(payload, payloadLen))
      {
        lastError_ = kErrOk;
        ok = buildAck(msgType, seq, kErrOk);
      }
      else
      {
        uint8_t err = lastError_;
        ok = buildAck(msgType, seq, err);
      }
      break;

    case kMsgSetWifiConfig:
      if (handleWifiConfigPayload(payload, payloadLen))
      {
        lastError_ = kErrOk;
        ok = buildAck(msgType, seq, kErrOk);
      }
      else
      {
        ok = buildAck(msgType, seq, lastError_);
      }
      break;

    case kMsgSetWsConfig:
      if (handleWsConfigPayload(payload, payloadLen))
      {
        lastError_ = kErrOk;
        ok = buildAck(msgType, seq, kErrOk);
      }
      else
      {
        ok = buildAck(msgType, seq, lastError_);
      }
      break;

    case kMsgGetLastResult:
      lastError_ = kErrOk;
      ok = buildStatusResponse(seq);
      break;

    default:
      lastError_ = kErrUnsupportedCmd;
      ok = buildAck(msgType, seq, kErrUnsupportedCmd);
      break;
  }

  return ok;
}

void Esp32CamApp::onI2CReceiveStatic(int count)
{
  if (instance_ != nullptr)
  {
    instance_->onI2CReceive(count);
  }
}

void Esp32CamApp::onI2CRequestStatic()
{
  if (instance_ != nullptr)
  {
    instance_->onI2CRequest();
  }
}

void Esp32CamApp::onWsEventStatic(WStype_t type, uint8_t *payload,
                                  size_t length)
{
  if (instance_ != nullptr)
  {
    instance_->onWsEvent(type, payload, length);
  }
}

void Esp32CamApp::onI2CReceive(int count)
{
  if (count <= 0)
  {
    return;
  }

  uint8_t raw[kMaxFrameLen] = {0};
  int index = 0;
  while (Wire.available() > 0 && index < (int)sizeof(raw))
  {
    raw[index++] = (uint8_t)Wire.read();
  }

  while (Wire.available() > 0)
  {
    (void)Wire.read();
  }

  (void)handleBinaryRequest(raw, (size_t)index);
}

void Esp32CamApp::onI2CRequest()
{
  if (pendingResponseLen_ >= kMinFrameLen)
  {
    Wire.write(pendingResponse_, pendingResponseLen_);
    pendingResponseLen_ = 0;
    return;
  }

  // Fallback to a full status frame when request arrives before a new command.
  if (buildStatusResponse(lastSeq_))
  {
    Wire.write(pendingResponse_, pendingResponseLen_);
    pendingResponseLen_ = 0;
  }
}

bool Esp32CamApp::parseWsUrl(const String &wsUrl, String &host, uint16_t &port,
                             String &path) const
{
  if (!wsUrl.startsWith("ws://"))
  {
    return false;
  }

  String rest = wsUrl.substring(5);
  int slashPos = rest.indexOf('/');
  String hostPort = slashPos >= 0 ? rest.substring(0, slashPos) : rest;
  path = slashPos >= 0 ? rest.substring(slashPos) : "/";

  int colonPos = hostPort.indexOf(':');
  if (colonPos < 0)
  {
    host = hostPort;
    port = 80;
    return host.length() > 0;
  }

  host = hostPort.substring(0, colonPos);
  port = (uint16_t)hostPort.substring(colonPos + 1).toInt();
  return host.length() > 0 && port > 0;
}

bool Esp32CamApp::parseProvisionJson(const String &json)
{
  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err)
  {
    Serial.printf("[I2C] JSON parse failed: %s\n", err.c_str());
    status_ = kStatusJsonError;
    return false;
  }

  const char *type = doc["type"] | "";
  if (strcmp(type, "provision") != 0)
  {
    status_ = kStatusJsonError;
    return false;
  }

  cfg_.ssid = String(doc["ssid"] | "");
  cfg_.password = String(doc["password"] | "");
  cfg_.wsUrl = String(doc["ws_url"] | "");
  cfg_.userId = (uint32_t)(doc["user_id"] | 0);
  cfg_.cameraId = (uint32_t)(doc["camera_id"] | APP_DEFAULT_CAMERA_ID);
  cfg_.valid = cfg_.ssid.length() > 0 && cfg_.wsUrl.length() > 0;

  if (!cfg_.valid)
  {
    status_ = kStatusJsonError;
    return false;
  }

  status_ = kStatusConfigReady;
  Serial.printf("[I2C] Config received: ssid=%s user_id=%u camera_id=%u\n",
                cfg_.ssid.c_str(), (unsigned)cfg_.userId,
                (unsigned)cfg_.cameraId);
  return true;
}

bool Esp32CamApp::connectWifi(const String &ssid, const String &password)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

#if APP_WIFI_USE_STATIC_IP
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;

  bool parseOk = ip.fromString(APP_WIFI_STATIC_IP) &&
                 gateway.fromString(APP_WIFI_STATIC_GATEWAY) &&
                 subnet.fromString(APP_WIFI_STATIC_SUBNET) &&
                 dns1.fromString(APP_WIFI_STATIC_DNS1) &&
                 dns2.fromString(APP_WIFI_STATIC_DNS2);

  if (!parseOk)
  {
    Serial.println("[WiFi] Static IP parse failed, fallback DHCP");
  }
  else
  {
    bool cfgOk = WiFi.config(ip, gateway, subnet, dns1, dns2);
    Serial.printf("[WiFi] Static IP %s (%s)\n", APP_WIFI_STATIC_IP,
                  cfgOk ? "applied" : "config failed, fallback DHCP");
  }
#endif

  WiFi.begin(ssid.c_str(), password.c_str());

  for (int i = 0; i < 40; ++i)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf("[WiFi] Connected, IP=%s\n",
                    WiFi.localIP().toString().c_str());
      status_ = kStatusWifiConnected;
      return true;
    }
    delay(500);
  }

  Serial.println("[WiFi] Connect timeout");
  status_ = kStatusWifiFailed;
  return false;
}

bool Esp32CamApp::initCameraPipeline()
{
  if (!photoStore_.begin())
  {
    Serial.println("[Camera] photo store init failed");
    return false;
  }

  esp_err_t err = cameraService_.begin();
  if (err != ESP_OK)
  {
    Serial.printf("[Camera] init failed: 0x%x\n", err);
    return false;
  }

  Serial.println("[Camera] init ok");
  return true;
}

void Esp32CamApp::onWsEvent(WStype_t type, uint8_t *payload, size_t length)
{
  (void)payload;
  (void)length;

  if (type == WStype_CONNECTED)
  {
    wsReady_ = true;
    status_ = kStatusWsConnected;
    Serial.println("[WS] Connected");
  }
  else if (type == WStype_DISCONNECTED)
  {
    wsReady_ = false;
    if (WiFi.status() == WL_CONNECTED)
    {
      status_ = kStatusWifiConnected;
    }
    Serial.println("[WS] Disconnected");
  }
}

bool Esp32CamApp::beginWsClient(const String &wsUrl)
{
  String host;
  String path;
  uint16_t port = 0;
  if (!parseWsUrl(wsUrl, host, port, path))
  {
    Serial.println("[WS] Invalid ws_url");
    return false;
  }

  ws_.begin(host.c_str(), port, path.c_str());
  ws_.onEvent(onWsEventStatic);
  ws_.setReconnectInterval(3000);
  return true;
}

void Esp32CamApp::startHttpPreviewServer()
{
  if (httpServerStarted_ || WiFi.status() != WL_CONNECTED || !cameraReady_)
  {
    return;
  }

  httpServer_.on("/", HTTP_GET,
                 []()
                 {
                   if (Esp32CamApp::instance_ != nullptr)
                   {
                     Esp32CamApp::instance_->onHttpRoot();
                   }
                 });

  httpServer_.on("/snapshot.jpg", HTTP_GET,
                 []()
                 {
                   if (Esp32CamApp::instance_ != nullptr)
                   {
                     Esp32CamApp::instance_->onHttpSnapshot();
                   }
                 });

  httpServer_.on("/health", HTTP_GET,
                 []()
                 {
                   if (Esp32CamApp::instance_ != nullptr)
                   {
                     Esp32CamApp::instance_->httpServer_.send(
                         200, "application/json", "{\"ok\":true}");
                   }
                 });

  httpServer_.begin();
  httpServerStarted_ = true;
  Serial.printf("[HTTP] Preview ready: http://%s/\n",
                WiFi.localIP().toString().c_str());
}

void Esp32CamApp::stopHttpPreviewServer()
{
  if (!httpServerStarted_)
  {
    return;
  }
  httpServer_.stop();
  httpServerStarted_ = false;
}

void Esp32CamApp::handleHttpServer()
{
  if (!httpServerStarted_)
  {
    return;
  }
  httpServer_.handleClient();
}

void Esp32CamApp::onHttpRoot()
{
  httpServer_.sendHeader("Cache-Control",
                         "no-store, no-cache, must-revalidate");
  httpServer_.send(200, "text/html", kPreviewHtml);
}

void Esp32CamApp::onHttpSnapshot()
{
  if (!cameraReady_)
  {
    httpServer_.send(503, "text/plain", "camera not ready");
    return;
  }

  uint8_t *buf = nullptr;
  size_t len = 0;
  uint64_t tsMs = 0;
  if (cameraService_.captureToJpegBuffer(&buf, &len, &tsMs) != ESP_OK ||
      buf == nullptr || len == 0)
  {
    httpServer_.send(500, "text/plain", "capture failed");
    return;
  }

  WiFiClient client = httpServer_.client();
  httpServer_.sendHeader("Cache-Control",
                         "no-store, no-cache, must-revalidate");
  httpServer_.setContentLength((int)len);
  httpServer_.send(200, "image/jpeg", "");
  (void)client.write(buf, len);
  free(buf);
}

void Esp32CamApp::sendFrameWithMeta()
{
  if (!wsReady_)
  {
    return;
  }

  if (millis() - lastFrameMs_ < 150)
  {
    return;
  }

  uint8_t *buf = nullptr;
  size_t len = 0;
  uint64_t tsMs = 0;
  if (cameraService_.captureToJpegBuffer(&buf, &len, &tsMs) != ESP_OK)
  {
    Serial.println("[Camera] capture failed");
    return;
  }

  StaticJsonDocument<160> meta;
  meta["type"] = "frame_meta";
  meta["user_id"] = cfg_.userId;
  meta["camera_id"] = cfg_.cameraId;
  meta["len"] = len;
  meta["ts"] = tsMs;

  char metaBuf[160];
  size_t n = serializeJson(meta, metaBuf, sizeof(metaBuf));
  ws_.sendTXT(metaBuf, n);
  ws_.sendBIN(buf, len);

  if (!photoStore_.pushOwnedFrame(buf, len, tsMs))
  {
    free(buf);
  }

  lastFrameMs_ = millis();
}

void Esp32CamApp::handleProvisionFlow()
{
  if (!configPending_)
  {
    return;
  }

  configPending_ = false;
  if (!parseProvisionJson(i2cBuffer_))
  {
    return;
  }

  if (!connectWifi(cfg_.ssid, cfg_.password))
  {
    return;
  }

  if (!cameraReady_)
  {
    cameraReady_ = initCameraPipeline();
  }

  if (cameraReady_)
  {
    startHttpPreviewServer();
    beginWsClient(cfg_.wsUrl);
  }
}

void Esp32CamApp::handleDirectConfigFlow()
{
  if (!pendingNetConfigApply_)
  {
    return;
  }

  pendingNetConfigApply_ = false;
  if (!cfg_.valid)
  {
    status_ = kStatusJsonError;
    return;
  }

  if (!connectWifi(cfg_.ssid, cfg_.password))
  {
    return;
  }

  if (!cameraReady_)
  {
    cameraReady_ = initCameraPipeline();
  }

  if (cameraReady_)
  {
    startHttpPreviewServer();
    beginWsClient(cfg_.wsUrl);
  }
}

void Esp32CamApp::processDeferredActions()
{
  if (!pendingReboot_)
  {
    return;
  }

  pendingReboot_ = false;
  delay(20);
  ESP.restart();
}

void Esp32CamApp::setup()
{
  instance_ = this;

  Serial.begin(115200);
  delay(300);

  Wire.begin((int)kI2CAddress, (int)kI2CSda, (int)kI2CScl, 100000);
  Wire.onReceive(onI2CReceiveStatic);
  Wire.onRequest(onI2CRequestStatic);

  Serial.printf("[BOOT] I2C slave address: 0x%02X\n", (unsigned)kI2CAddress);
  Serial.printf("[BOOT] Default camera_id: %u\n",
                (unsigned)APP_DEFAULT_CAMERA_ID);
  Serial.println("[BOOT] ESP32-CAM waiting for IIC provision payload...");
}

void Esp32CamApp::loop()
{
  handleProvisionFlow();
  handleDirectConfigFlow();
  processDeferredActions();

  if (WiFi.status() != WL_CONNECTED && cfg_.valid)
  {
    wsReady_ = false;
    stopHttpPreviewServer();
    connectWifi(cfg_.ssid, cfg_.password);
  }

  if (WiFi.status() == WL_CONNECTED && cameraReady_)
  {
    startHttpPreviewServer();
  }

  handleHttpServer();
  ws_.loop();
  sendFrameWithMeta();
  delay(10);
}
