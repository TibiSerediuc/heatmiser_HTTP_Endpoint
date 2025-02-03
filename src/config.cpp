#include <EEPROM.h>
#include "globals.h"

void saveConfig()
{
  EEPROM.put(0, config);
  EEPROM.commit();
}

// EEPROM functions for configuration management
void loadConfig()
{
  EEPROM.get(0, config);

  // Check if config is valid (simple validation)
  if (config.wifi_ssid[0] == 0xFF)
  {
    // First time running, initialize config
    memset(&config, 0, sizeof(config));
    config.isConfigured = false;
    saveConfig();
  }
}

void handleRoot()
{
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "input { width: 100%; padding: 12px 20px; margin: 8px 0; box-sizing: border-box; }";
  html += "input[type=submit] { background-color: #4CAF50; color: white; border: none; cursor: pointer; }";
  html += "input[type=submit]:hover { background-color: #45a049; }";
  html += ".container { max-width: 500px; margin: auto; }";
  html += ".info { background-color: #f8f9fa; padding: 15px; border-radius: 4px; margin-bottom: 20px; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Heatmiser Neo Setup</h1>";
  html += "<div class='info'>";
  html += "<p>This device will be accessible as: <strong>" + String(DEVICE_HOSTNAME) + "</strong></p>";
  html += "<p>You can find it on your network using this hostname or by scanning for this name.</p>";
  html += "</div>";
  html += "<form method='post' action='/configure'>";
  html += "<label for='ssid'>WiFi Network Name:</label>";
  html += "<input name='ssid' type='text' required><br>";
  html += "<label for='password'>WiFi Password:</label>";
  html += "<input name='password' type='password' required><br>";
  html += "<label for='heatmiser_ip'>Heatmiser IP Address:</label>";
  html += "<input name='heatmiser_ip' type='text' required placeholder='e.g., 192.168.1.100'><br>";
  html += "<label for='api_key'>Heatmiser API Key:</label>";
  html += "<input name='api_key' type='text' required><br>";
  html += "<input type='submit' value='Save Configuration'>";
  html += "</form>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleConfigure()
{
  if (server.hasArg("ssid") && server.hasArg("password") &&
      server.hasArg("heatmiser_ip") && server.hasArg("api_key"))
  {

    strncpy(config.wifi_ssid, server.arg("ssid").c_str(), sizeof(config.wifi_ssid));
    strncpy(config.wifi_password, server.arg("password").c_str(), sizeof(config.wifi_password));
    strncpy(config.heatmiser_ip, server.arg("heatmiser_ip").c_str(), sizeof(config.heatmiser_ip));
    strncpy(config.api_key, server.arg("api_key").c_str(), sizeof(config.api_key));
    config.isConfigured = true;

    saveConfig();

    server.send(200, "text/plain", "Configuration saved. Device will restart...");
    delay(2000);
    ESP.restart();
  }
  else
  {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void startConfigMode()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // Configure captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/configure", HTTP_POST, handleConfigure);

  // Add handler for captive portal
  server.onNotFound([]()
                    {
    server.sendHeader("Location", "http://192.168.4.1", true);
    server.send(302, "text/plain", ""); });

  server.begin();

  configModeStartTime = millis();
  Serial.println("Configuration mode started");
  Serial.println("Connect to WiFi network: " + String(AP_SSID));
  Serial.println("Then access: http://192.168.4.1");
}

void handleReset()
{
  Serial.println("Reset endpoint called");

  memset(&config, 0, sizeof(config));
  config.isConfigured = false;
  saveConfig();

  String response = "Configuration cleared. Device will restart in 2 seconds...";
  Serial.println(response);
  server.send(200, "text/plain", response);

  delay(2000);
  ESP.restart();
}
