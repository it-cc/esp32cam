#include <Arduino.h>
#include <WiFi.h>

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

void setup()
{
  // Keep the IIC slave object alive for the full application lifetime.
  static esp32camera::CameraIIC cameraIIC(CAMERA_IIC_ADDRESS, IIC_SDA_PIN,
                                          IIC_SCL_PIN, IIC_FREQUENCY);
  (void)cameraIIC;

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    esp32camera::CameraPackage cameraPackage = cameraIIC.getCameraPackage();
    WiFi.begin(cameraPackage.ssid, cameraPackage.password);
    delay(2000);
    Serial.print(".");
  }
  cameraIIC.instance_->salveStatus_.isSetWifi = 0x02;
  strncpy(cameraIIC.instance_->salveStatus_.httpUrl, "http://",
          sizeof(cameraIIC.instance_->salveStatus_.httpUrl));
  strcat(cameraIIC.instance_->salveStatus_.httpUrl,
         WiFi.localIP().toString().c_str());
  Serial.println("Connected to WiFi!");
  Serial.print("Access the camera stream at: http://");
  Serial.println(WiFi.localIP());

  // Start the camera web server.
  cameraInit();
}

void loop() { delay(10000); }