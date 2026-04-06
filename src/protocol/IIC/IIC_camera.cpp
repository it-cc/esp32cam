#include "protocol/IIC/IIC_camera.h"

namespace esp32camera
{
CameraIIC* CameraIIC::instance_ = nullptr;

CameraIIC::CameraIIC(uint8_t slaveAddress, int sdaPin, int sclPin,
                     uint32_t frequency)
    : cameraPacket_(), salveStatus_(), isReceived_(false)
{
  instance_ = this;
  Wire.begin(slaveAddress, sdaPin, sclPin, frequency);
  Wire.onRequest(onRequestCallback);
  Wire.onReceive(onReceiveCallback);
}

void CameraIIC::onRequestCallback()
{
  if (instance_ == nullptr)
  {
    return;
  }

  Wire.write((const uint8_t*)&instance_->salveStatus_, sizeof(SalveStatus));
}

void CameraIIC::onReceiveCallback(int numBytes)
{
  if (instance_ == nullptr)
  {
    while (Wire.available())
    {
      Wire.read();
    }
    return;
  }

  if (numBytes == sizeof(CameraPackage))
  {
    uint8_t* p = (uint8_t*)&instance_->cameraPacket_;
    while (Wire.available())
    {
      *p++ = Wire.read();
    }
    instance_->salveStatus_.isReceived = 0x01;
    instance_->salveStatus_.isSetWifi = 0x02;
    instance_->salveStatus_.isgetUserID = 0x04;

    instance_->isReceived_ = true;
  }
  else
  {
    while (Wire.available())
    {
      Wire.read();
    }
  }
}

CameraPackage CameraIIC::getCameraPackage() const
{
  return cameraPacket_;
}

}  // namespace esp32camera
