#include <EEPROM.h>
#include "globals.h"
#include "config.h"
#include "websockets.h"
#include "webserver.h"
#include "network.h"

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  // Load configuration from EEPROM
  loadConfig();

  if (!config.isConfigured)
  {
    startConfigMode();
  }
  else
  {
    connectToNetwork();
    setupWebSocket();
    setupHttpServer();
  }
}

void loop()
{
  if (isConfigMode)
  {
    dnsServer.processNextRequest();
    server.handleClient();

    // Check if config mode timeout reached
    if (millis() - configModeStartTime > CONFIG_MODE_TIMEOUT)
    {
      ESP.restart();
    }
  }
  else
  {

    webSocket.loop();
    server.handleClient();
  }
}