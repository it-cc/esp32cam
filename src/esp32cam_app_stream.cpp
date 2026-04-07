#include "esp32cam_app.h"

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

void Esp32CamApp::sendFrameWithMeta()
{
  if (!cameraReady_ || !webServerStarted_)
  {
    return;
  }

  if (millis() - lastFrameMs_ < 100)
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

  if (photoStore_.pushOwnedFrame(buf, len, tsMs))
  {
    photoWebServer_.notifyNewFrame();
  }
  else
  {
    free(buf);
  }

  lastFrameMs_ = millis();
}
