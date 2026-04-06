#include <Arduino.h>

#include "esp32cam_app.h"

namespace
{
Esp32CamApp g_app;
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println("setup begin");
}

void loop() {}