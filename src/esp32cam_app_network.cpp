#include <WiFi.h>

#include "esp32cam_app.h"


#ifndef APP_WIFI_USE_STATIC_IP
#define APP_WIFI_USE_STATIC_IP 0
#endif

#ifndef APP_WIFI_STATIC_IP
#define APP_WIFI_STATIC_IP "192.168.1.123"
#endif

#ifndef APP_WIFI_STATIC_GATEWAY
#define APP_WIFI_STATIC_GATEWAY "192.168.1.1"
#endif

#ifndef APP_WIFI_STATIC_SUBNET
#define APP_WIFI_STATIC_SUBNET "255.255.255.0"
#endif

#ifndef APP_WIFI_STATIC_DNS1
#define APP_WIFI_STATIC_DNS1 "8.8.8.8"
#endif

#ifndef APP_WIFI_STATIC_DNS2
#define APP_WIFI_STATIC_DNS2 "1.1.1.1"
#endif

bool Esp32CamApp::connectWifi(const String &ssid, const String &password)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

#if APP_WIFI_USE_STATIC_IP
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;

  bool parseOk = ip.fromString(APP_WIFI_STATIC_IP) &&
                 gateway.fromString(APP_WIFI_STATIC_GATEWAY) &&
                 subnet.fromString(APP_WIFI_STATIC_SUBNET) &&
                 dns1.fromString(APP_WIFI_STATIC_DNS1) &&
                 dns2.fromString(APP_WIFI_STATIC_DNS2);

  if (!parseOk)
  {
    Serial.println("[WiFi] Static IP parse failed, fallback DHCP");
  }
  else
  {
    bool cfgOk = WiFi.config(ip, gateway, subnet, dns1, dns2);
    Serial.printf("[WiFi] Static IP %s (%s)\n", APP_WIFI_STATIC_IP,
                  cfgOk ? "applied" : "config failed, fallback DHCP");
  }
#endif

  WiFi.begin(ssid.c_str(), password.c_str());

  for (int i = 0; i < 40; ++i)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf("[WiFi] Connected, IP=%s\n",
                    WiFi.localIP().toString().c_str());
      status_ = kStatusWifiConnected;
      return true;
    }
    delay(500);
  }

  Serial.println("[WiFi] Connect timeout");
  status_ = kStatusWifiFailed;
  return false;
}
