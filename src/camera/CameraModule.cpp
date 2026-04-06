#include "camera/CameraModule.h"

#include <Arduino.h>

#include "LogSwitch.h"
#include "camera/CameraService.h"
#include "camera/MemoryPhotoStore.h"

namespace esp32s3
{
static CameraService g_camera;
static MemoryPhotoStore g_photoStore(6);

static bool s_initialized = false;
}  // namespace esp32s3

bool esp32s3::CameraModule::init()
{
  if (!esp32s3::g_photoStore.begin())
  {
    LOG_PRINTLN(LOG_CAMERA, "Photo store init failed");
    return false;
  }

  if (esp32s3::g_camera.begin() != ESP_OK)
  {
    LOG_PRINTLN(LOG_CAMERA, "Camera init failed");
    return false;
  }

  LOG_PRINTLN(LOG_CAMERA, "Camera init success");
  esp32s3::s_initialized = true;
  return true;
}

bool esp32s3::CameraModule::startTasks()
{
  if (!esp32s3::s_initialized)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera init required before start tasks");
    return false;
  }
  // Legacy API compatibility: camera tasks are now owned by Esp32CamApp.
  return true;
}

bool esp32s3::CameraModule::handleBleCommand(const String& data)
{
  (void)data;
  return false;
}
