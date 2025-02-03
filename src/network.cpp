#include "websockets.h"
#include "webserver.h"
#include "config.h"
#include "globals.h"
#include "network.h"

void connectToNetwork()
{
  WiFi.mode(WIFI_STA);

  // Set hostname for easy identification
  WiFi.setHostname(DEVICE_HOSTNAME);

  WiFi.begin(config.wifi_ssid, config.wifi_password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    isConfigMode = false;
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());

    setupWebSocket();
    setupHttpServer();
  }
  else
  {
    startConfigMode(); // Fall back to config mode if connection fails
  }
}