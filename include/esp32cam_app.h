#pragma once

#include <Arduino.h>

#include "camera/CameraService.h"
#include "camera/MemoryPhotoStore.h"
#include "camera/PhotoWebServer.h"

#ifndef APP_DEFAULT_CAMERA_ID
#define APP_DEFAULT_CAMERA_ID 0
#endif

class Esp32CamApp
{
 public:
  void setup();
  void loop();

 private:
  // Configuration layer: external provisioning and identity.
  struct RuntimeConfig
  {
    String ssid;
    String password;
    uint32_t userId;
    uint32_t cameraId;
    bool valid;
  };

  // Runtime state layer: mutable app status and flags.
  struct RuntimeState
  {
    uint8_t status;
    bool cameraReady;
    bool webServerStarted;
    unsigned long lastFrameMs;
  };

  // Service layer: long-lived subsystem objects.
  struct ServiceHub
  {
    CameraService cameraService;
    MemoryPhotoStore photoStore{6};
    PhotoWebServer photoWebServer{photoStore};
  };

  static constexpr uint8_t kStatusConfigReady = 1;
  static constexpr uint8_t kStatusWifiConnected = 2;
  static constexpr uint8_t kStatusJsonError = 0xE0;
  static constexpr uint8_t kStatusWifiFailed = 0xE1;

  static Esp32CamApp *instance_;

  RuntimeConfig cfg_ = {"", "", 0, APP_DEFAULT_CAMERA_ID, false};
  RuntimeState runtime_ = {0, false, false, 0};
  ServiceHub services_;

  // Compatibility aliases keep existing .cpp logic unchanged.
  uint8_t &status_ = runtime_.status;
  bool &cameraReady_ = runtime_.cameraReady;
  bool &webServerStarted_ = runtime_.webServerStarted;
  unsigned long &lastFrameMs_ = runtime_.lastFrameMs;

  CameraService &cameraService_ = services_.cameraService;
  MemoryPhotoStore &photoStore_ = services_.photoStore;
  PhotoWebServer &photoWebServer_ = services_.photoWebServer;

  bool connectWifi(const String &ssid, const String &password);
  bool initCameraPipeline();
  void sendFrameWithMeta();
};
