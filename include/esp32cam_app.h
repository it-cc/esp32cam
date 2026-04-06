#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsClient.h>

#include "camera/CameraService.h"
#include "camera/MemoryPhotoStore.h"

#ifndef APP_I2C_ADDR
#define APP_I2C_ADDR 0x24
#endif

#ifndef APP_DEFAULT_CAMERA_ID
#define APP_DEFAULT_CAMERA_ID 0
#endif

class Esp32CamApp
{
 public:
  void setup();
  void loop();

 private:
  struct RuntimeConfig
  {
    String ssid;
    String password;
    String wsUrl;
    uint32_t userId;
    uint32_t cameraId;
    bool valid;
  };

  static constexpr uint8_t kI2CAddress = APP_I2C_ADDR;
  static constexpr uint8_t kI2CSda = 14;
  static constexpr uint8_t kI2CScl = 15;

  static constexpr uint8_t kFrameStart = 0x01;
  static constexpr uint8_t kFrameChunk = 0x02;
  static constexpr uint8_t kFrameEnd = 0x03;

  static constexpr uint8_t kStatusIdle = 0;
  static constexpr uint8_t kStatusConfigReady = 1;
  static constexpr uint8_t kStatusWifiConnected = 2;
  static constexpr uint8_t kStatusWsConnected = 3;
  static constexpr uint8_t kStatusJsonError = 0xE0;
  static constexpr uint8_t kStatusWifiFailed = 0xE1;

  static constexpr uint8_t kProtocolVersion = 1;
  static constexpr uint8_t kMaxPayloadLen = 48;
  static constexpr uint8_t kMinFrameLen = 5;
  static constexpr uint8_t kMaxFrameLen = kMinFrameLen + kMaxPayloadLen;

  enum MessageType : uint8_t
  {
    kMsgPing = 0x01,
    kMsgGetStatus = 0x02,
    kMsgTriggerCapture = 0x03,
    kMsgRebootNode = 0x04,
    kMsgGetLastResult = 0x05,
    kMsgSetWifiConfig = 0x06,
    kMsgSetWsConfig = 0x07,
    kMsgProvisionFrame = 0x08,
  };

  enum ErrorCode : uint8_t
  {
    kErrOk = 0x00,
    kErrBadCrc = 0x01,
    kErrBadLen = 0x02,
    kErrUnsupportedCmd = 0x03,
    kErrBusy = 0x04,
    kErrInternal = 0x05,
  };

  static Esp32CamApp *instance_;

  volatile uint8_t status_ = kStatusIdle;
  uint8_t lastError_ = kErrOk;
  uint8_t lastSeq_ = 0;
  uint16_t frameCounter_ = 0;

  uint8_t pendingResponse_[kMaxFrameLen] = {0};
  uint8_t pendingResponseLen_ = 0;
  bool pendingReboot_ = false;
  bool pendingNetConfigApply_ = false;

  String i2cBuffer_;
  bool configPending_ = false;

  RuntimeConfig cfg_ = {"", "", "", 0, APP_DEFAULT_CAMERA_ID, false};
  WebSocketsClient ws_;
  WebServer httpServer_{80};
  CameraService cameraService_;
  MemoryPhotoStore photoStore_{6};
  bool cameraReady_ = false;
  bool wsReady_ = false;
  bool httpServerStarted_ = false;
  unsigned long lastFrameMs_ = 0;

  static void onI2CReceiveStatic(int count);
  static void onI2CRequestStatic();
  static void onWsEventStatic(WStype_t type, uint8_t *payload, size_t length);

  void onI2CReceive(int count);
  void onI2CRequest();
  void onWsEvent(WStype_t type, uint8_t *payload, size_t length);

  static uint8_t crc8(const uint8_t *data, size_t len);
  static bool decodeFrame(const uint8_t *data, size_t len, uint8_t &msgType,
                          uint8_t &seq, const uint8_t *&payload,
                          uint8_t &payloadLen, uint8_t &decodeErr);
  bool encodeResponse(uint8_t msgType, uint8_t seq, const uint8_t *payload,
                      uint8_t payloadLen);
  bool buildAck(uint8_t msgType, uint8_t seq, uint8_t errCode);
  bool buildStatusResponse(uint8_t seq);
  bool handleProvisionPayload(const uint8_t *payload, uint8_t payloadLen);
  bool handleWifiConfigPayload(const uint8_t *payload, uint8_t payloadLen);
  bool handleWsConfigPayload(const uint8_t *payload, uint8_t payloadLen);
  bool handleBinaryRequest(const uint8_t *data, size_t len);

  bool parseWsUrl(const String &wsUrl, String &host, uint16_t &port,
                  String &path) const;
  bool parseProvisionJson(const String &json);
  bool connectWifi(const String &ssid, const String &password);
  bool initCameraPipeline();
  bool beginWsClient(const String &wsUrl);
  void startHttpPreviewServer();
  void stopHttpPreviewServer();
  void handleHttpServer();
  void onHttpRoot();
  void onHttpSnapshot();
  void sendFrameWithMeta();
  void handleProvisionFlow();
  void handleDirectConfigFlow();
  void processDeferredActions();
};
