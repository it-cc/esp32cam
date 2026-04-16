#include <Arduino.h>
#include <WiFi.h>

#include "camera/CameraWebserver.h"
#include "config/app_config.h"
#include "protocol/http/http_client.h"
#include "protocol/webSocket/webSocket_client.h"

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(esp32camera::WifiSsid.c_str(), esp32camera::WifiPassword.c_str());
  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to WiFi!");
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());

  // Initialize camera only; skip local web server when running as HTTP client.
  cameraInit(false);

  // http client test
  static esp32camera::clientConfig clientCfg{
      .serverURL = esp32camera::serverURL,
      .headValue = esp32camera::headValue,
      .queryKey = esp32camera::queryKey,
      .queryValue = esp32camera::queryValue,
  };
  static esp32camera::cameraClient camClient(clientCfg);

  // websocket client test
  // static esp32camera::WebsocketClient webSocketClient(
  //     esp32camera::webSocket_host, esp32camera::webSocket_port,
  //     esp32camera::webSocket_path);
}

void loop() {}