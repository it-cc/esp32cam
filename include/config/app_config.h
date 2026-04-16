#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <Arduino.h>
#include <HTTPClient.h>

namespace esp32camera
{
static const String WifiSsid = "Redmi";
static const String WifiPassword = "88889999";
static const String serverURL = "http://10.243.144.156:8080/ai/camera";
static const String headValue = "1";
static const String queryKey = "position";
static const int queryValue = 0;

static const char* webSocket_host = "10.243.144.156";
static const int webSocket_port = 8080;
const char* webSocket_path = "/ai/camera";
}  // namespace esp32camera

#endif  // APP_CONFIG_H
