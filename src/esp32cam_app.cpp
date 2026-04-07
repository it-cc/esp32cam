#include "esp32cam_app.h"

#include <WiFi.h>

Esp32CamApp *Esp32CamApp::instance_ = nullptr;

void Esp32CamApp::setup()
{
  instance_ = this;

  Serial.begin(115200);
  delay(300);
  Serial.printf("[BOOT] Default camera_id: %u\n",
                (unsigned)APP_DEFAULT_CAMERA_ID);
}

void Esp32CamApp::loop()
{
  if (WiFi.status() != WL_CONNECTED && cfg_.valid)
  {
    webServerStarted_ = false;
    connectWifi(cfg_.ssid, cfg_.password);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!cameraReady_)
    {
      cameraReady_ = initCameraPipeline();
    }

    if (cameraReady_ && !webServerStarted_)
    {
      photoWebServer_.setUserId(cfg_.userId);
      webServerStarted_ = photoWebServer_.begin(80);
    }
  }

  sendFrameWithMeta();
  delay(10);
}
