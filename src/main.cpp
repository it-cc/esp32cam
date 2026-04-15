#include <Arduino.h>
#include <WiFi.h>

#include "WiFi.h"
#include "camera/CameraWebserver.h"
#include "protocol/IIC/IIC_camera.h"
#define CAMERA 1
#if CAMERA == 1
#define CAMERA_IIC_ADDRESS 0x42
#elif CAMERA == 2
#define CAMERA_IIC_ADDRESS 0x43
#endif

#define IIC_SCL_PIN 15
#define IIC_SDA_PIN 14
#define IIC_FREQUENCY 100000

static const char *kWifiSsid = "Redmi";
static const char *kWifiPassword = "88889999";
static const uint32_t kWifiConnectTimeoutMs = 30000;

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiSsid, kWifiPassword);
  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.printf("Connecting to WiFi (%s), status=%d ...\n", kWifiSsid,
                  WiFi.status());
    if (millis() - startMs > kWifiConnectTimeoutMs)
    {
      Serial.println("WiFi connect timeout, restarting...");
      ESP.restart();
    }
    delay(1000);
  }
  Serial.println("Connected to WiFi!");
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  // Start the camera web server.
  cameraInit();
}

void loop() { delay(10000); }