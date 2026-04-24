#include <Arduino.h>
#include <WiFi.h>

#include "camera/CameraWebserver.h"
#include "config/app_config.h"
#include "protocol/IIC/IIC_camera.h"
#include "protocol/http/http_client.h"
#include "protocol/webSocket/webSocket_client.h"

#define CAMERA_IIC_ADDRESS 0x42
#define IIC_SCL_PIN 15
#define IIC_SDA_PIN 14
#define IIC_FREQUENCY 100000

void setup()
{
  Serial.begin(115200);

  static esp32camera::CameraIIC cameraIIC(CAMERA_IIC_ADDRESS, IIC_SDA_PIN,
                                          IIC_SCL_PIN, IIC_FREQUENCY);
  (void)cameraIIC;
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to WiFi");

  while (cameraIIC.getSalveStatus().isAllReady != 0x02)
  {
    delay(500);
    Serial.println("Waiting for camera package...");
  }
  esp32camera::CameraPackage cameraPackage = cameraIIC.getCameraPackage();
  WiFi.begin(cameraPackage.ssid, cameraPackage.password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Initialize camera only; skip local web server when running as HTTP client.
  bool cameraInitialized = cameraInit(false);
  if (!cameraInitialized)
  {
    Serial.println("Camera initialization failed");
    return;
  }
  else
  {
    Serial.println("Camera initialization succeeded");
  }
  // websocket client test
  if (cameraInitialized)
  {
    static esp32camera::WebsocketClient webSocketClient(
        esp32camera::webSocket_host, esp32camera::webSocket_port,
        esp32camera::webSocket_path);
  }
}

void loop() {}