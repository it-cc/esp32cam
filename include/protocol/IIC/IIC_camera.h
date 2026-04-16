#ifndef IIC_CAMERA_H
#define IIC_CAMERA_H

#include <Wire.h>

#include <cstdint>

namespace esp32camera
{
struct __attribute__((packed)) CameraPackage
{
  uint8_t userID;
  char ssid[32];
  char password[32];
};
struct __attribute__((packed)) SalveStatus
{
  uint8_t isReceived;  // 0x01
  uint8_t isAllReady;  // 0x02
  char ssid[32];
  char password[32];
  char httpUrl[64];
};
class CameraIIC
{
 public:
  CameraIIC(uint8_t slaveAddress, int sdaPin, int sclPin, uint32_t frequency);
  CameraPackage getCameraPackage() const;
  static CameraIIC* instance_;
  SalveStatus salveStatus_;

 private:
  static void onRequestCallback();
  static void onReceiveCallback(int numBytes);

  CameraPackage cameraPacket_;
};
}  // namespace esp32camera
#endif  // IIC_CAMERA_H