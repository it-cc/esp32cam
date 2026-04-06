#include <Arduino.h>
#include <WiFi.h>

#include "protocol/IIC/IIC_camera.h"

#define CAMERA1_IIC_ADDRESS 0x42
#define CAMERA2_IIC_ADDRESS 0x43
#define IIC_SCL_PIN 15
#define IIC_SDA_PIN 14
#define IIC_FREQUENCY 100000

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println("setup begin");

  // Keep the IIC slave object alive for the full application lifetime.
  static esp32camera::CameraIIC cameraIIC(CAMERA1_IIC_ADDRESS, IIC_SDA_PIN,
                                          IIC_SCL_PIN, IIC_FREQUENCY);
  (void)cameraIIC;
  esp32camera::CameraPackage cameraPackage = cameraIIC.getCameraPackage();

  WiFi.begin(cameraPackage.ssid, cameraPackage.password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("Connected to WiFi");
}

void loop() { delay(10); }