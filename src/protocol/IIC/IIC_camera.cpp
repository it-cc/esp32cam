#include "protocol/IIC/IIC_camera.h"

namespace esp32camera
{
CameraIIC* CameraIIC::instance_ = nullptr;

CameraIIC::CameraIIC(uint8_t slaveAddress, int sdaPin, int sclPin,
                     uint32_t frequency)
    : cameraPacket_(), salveStatus_()
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
    instance_->salveStatus_.isReceived = 0x01;  // Mark as received

    // Read the incoming data directly into the cameraPacket_ structure
    uint8_t* p = (uint8_t*)&instance_->cameraPacket_;
    while (Wire.available())
    {
      *p++ = Wire.read();
    }

    instance_->salveStatus_.isAllReady = 0x02;  // Mark as all ready
  }
  else
  {
    while (Wire.available())
    {
      Wire.read();
    }
  }
}

CameraPackage CameraIIC::getCameraPackage() const { return cameraPacket_; }

SalveStatus CameraIIC::getSalveStatus() const { return salveStatus_; }

}  // namespace esp32camera
