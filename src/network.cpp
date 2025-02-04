#include "websockets.h"
#include "webserver.h"
#include "config.h"
#include "globals.h"
#include "network.h"

void connectToNetwork()
{
  // Make sure AP is fully disabled
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  // Now switch to station mode
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);

  Serial.println("[NET] Connecting to WiFi...");
  Serial.println("[NET] SSID: " + String(config.wifi_ssid));
  
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
    Serial.println("\n[NET] Connected to WiFi");
    Serial.print("[NET] IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[NET] Hostname: ");
    Serial.println(WiFi.getHostname());

    // Set up HTTP server first
    setupHttpServer();
    
    // Then set up WebSocket
    setupWebSocket();
    
    // Wait for initial WebSocket connection
    int wsAttempts = 0;
    while (!webSocket.isConnected() && wsAttempts < 20)
    {
      delay(500);
      webSocket.loop();
      Serial.print("w");
      wsAttempts++;
    }

    if (!webSocket.isConnected())
    {
      Serial.println("\n[NET] Failed to establish WebSocket connection");
      startConfigMode(); // Fall back to config mode if WebSocket fails
    }
    else
    {
      Serial.println("\n[NET] WebSocket connection established");
    }
  }
  else
  {
    Serial.println("\n[NET] Failed to connect to WiFi");
    startConfigMode(); // Fall back to config mode if connection fails
  }
}