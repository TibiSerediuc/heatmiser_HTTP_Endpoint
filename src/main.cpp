#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsClient.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// Constants
#define EEPROM_SIZE 512
#define AP_SSID "HeatmiserSetup"
#define AP_PASSWORD "12345678"
#define DNS_PORT 53
#define CONFIG_MODE_TIMEOUT 300000  // 5 minutes in milliseconds
#define HEATMISER_PORT 4243  // Correct Heatmiser port
#define DEVICE_HOSTNAME "heatmiser-bridge"  // Default hostname

// Global variables
WebServer server(80);
WebSocketsClient webSocket;
DNSServer dnsServer;
bool isConfigMode = true;
unsigned long configModeStartTime;

// Configuration structure
struct Config {
  char wifi_ssid[32];
  char wifi_password[64];
  char heatmiser_ip[16];
  char api_key[64];
  bool isConfigured;
} config;

// Function signatures
void setup();
void loop();
void startConfigMode();
void handleRoot();
void handleConfigure();
void connectToNetwork();
void setupWebSocket();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void sendCommand();  // Add function signature for sendCommand
void handleWebSocketMessage(uint8_t * payload, size_t length);
void loadConfig();
void saveConfig();
void setupHttpServer();
void handleStatus();
void handleSetTemperature();
void handleGetTemperature();
void handleReset();
void handleGetZones();
void checkWebSocketConnection();

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  
  // Load configuration from EEPROM
  loadConfig();
  
  if (!config.isConfigured) {
    startConfigMode();
  } else {
    connectToNetwork();
  }
}

void loop() {
  if (isConfigMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Check if config mode timeout reached
    if (millis() - configModeStartTime > CONFIG_MODE_TIMEOUT) {
      ESP.restart();
    }
  } else {

    webSocket.loop();
    server.handleClient();
  }
}

void startConfigMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  // Configure captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/configure", HTTP_POST, handleConfigure);
  
  // Add handler for captive portal
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1", true);
    server.send(302, "text/plain", "");
  });
  
  server.begin();
  
  configModeStartTime = millis();
  Serial.println("Configuration mode started");
  Serial.println("Connect to WiFi network: " + String(AP_SSID));
  Serial.println("Then access: http://192.168.4.1");
}

void handleRoot() {
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

void handleConfigure() {
  if (server.hasArg("ssid") && server.hasArg("password") && 
      server.hasArg("heatmiser_ip") && server.hasArg("api_key")) {
    
    strncpy(config.wifi_ssid, server.arg("ssid").c_str(), sizeof(config.wifi_ssid));
    strncpy(config.wifi_password, server.arg("password").c_str(), sizeof(config.wifi_password));
    strncpy(config.heatmiser_ip, server.arg("heatmiser_ip").c_str(), sizeof(config.heatmiser_ip));
    strncpy(config.api_key, server.arg("api_key").c_str(), sizeof(config.api_key));
    config.isConfigured = true;
    
    saveConfig();
    
    server.send(200, "text/plain", "Configuration saved. Device will restart...");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void connectToNetwork() {
  WiFi.mode(WIFI_STA);
  
  // Set hostname for easy identification
  WiFi.setHostname(DEVICE_HOSTNAME);
  
  WiFi.begin(config.wifi_ssid, config.wifi_password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    isConfigMode = false;
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());
    
    setupWebSocket();
    setupHttpServer();
  } else {
    startConfigMode();  // Fall back to config mode if connection fails
  }
}

void setupWebSocket() {
  Serial.println("Setting up WebSocket connection...");
  Serial.println("Heatmiser IP: " + String(config.heatmiser_ip));
  Serial.println("Heatmiser Port: " + String(HEATMISER_PORT));
  
  // Begin WebSocket connection with SSL
  webSocket.beginSSL(config.heatmiser_ip, HEATMISER_PORT, "/");
  
  webSocket.onEvent(webSocketEvent);

  Serial.println("WebSocket setup completed");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket Disconnected!");
      break;
      
    case WStype_CONNECTED:
      Serial.println("WebSocket Connected!");
      sendCommand();
      break;
      
    case WStype_TEXT:
      Serial.println("Received message: " + String((char*)payload));
      handleWebSocketMessage(payload, length);
      break;
      
    case WStype_ERROR:
      Serial.println("WebSocket Error!");
      if(payload) {
        Serial.println("Error payload: " + String((char*)payload));
      }
      break;
      
    case WStype_BIN:
      Serial.println("Received binary data");
      break;
      
    case WStype_PING:
      Serial.println("Received ping");
      break;
      
    case WStype_PONG:
      Serial.println("Received pong");
      break;
      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      Serial.println("Received fragmented data");
      break;
  }
}

void sendCommand() {
    String command = "{\"message_type\":\"hm_get_command_queue\",\"message\":\"{\\\"token\\\":\\\"4369f7c9-b6d3-4bda-9b2c-03145426ad78\\\",\\\"COMMANDS\\\":[{\\\"COMMAND\\\":\\\"{'GET_ZONES': 1}\\\",\\\"COMMANDID\\\":2}]}\"}";
    webSocket.sendTXT(command);
}


void handleWebSocketMessage(uint8_t * payload, size_t length) {
  Serial.println("Parsing message...");
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Log the entire message for debugging
  String debugMsg;
  serializeJson(doc, debugMsg);
  Serial.println("Received JSON: " + debugMsg);
  
  // Handle different message types
  const char* messageType = doc["message_type"];
  if (messageType) {
    Serial.println("Message type: " + String(messageType));
    
    if (strcmp(messageType, "hm_response") == 0) {
      // Handle response
      JsonObject message = doc["message"];
      if (!message.isNull()) {
        // Process the response
        // Add specific handling based on the response structure
      }
    }
  }
}

// EEPROM functions for configuration management
void loadConfig() {
  EEPROM.get(0, config);
  
  // Check if config is valid (simple validation)
  if (config.wifi_ssid[0] == 0xFF) {
    // First time running, initialize config
    memset(&config, 0, sizeof(config));
    config.isConfigured = false;
    saveConfig();
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void setupHttpServer() {
  // Setup endpoints for Loxone integration
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/settemp", HTTP_POST, handleSetTemperature);
  server.on("/gettemp", HTTP_GET, handleGetTemperature);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/test/getzones", HTTP_GET, handleGetZones);
  
  // Add a root handler for normal mode
  server.on("/", HTTP_GET, []() {
    String status = "Device is running normally<br>";
    status += "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "<br>";
    status += "WebSocket: " + String(webSocket.isConnected() ? "connected" : "disconnected") + "<br>";
    status += "<br><a href='/reset'>Reset Configuration</a>";
    server.send(200, "text/html", status);
  });
  
  server.begin();
  Serial.println("HTTP server started");
}

void handleStatus() {
  String status = "{ \"wifi\": \"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\", ";
  status += "\"websocket\": \"" + String(webSocket.isConnected() ? "connected" : "disconnected") + "\", ";
  status += "\"ip\": \"" + WiFi.localIP().toString() + "\", ";
  status += "\"heatmiser_ip\": \"" + String(config.heatmiser_ip) + "\", ";
  status += "\"heatmiser_port\": " + String(HEATMISER_PORT) + ", ";
  status += "\"hostname\": \"" + String(DEVICE_HOSTNAME) + "\", ";
  status += "\"config_mode\": " + String(isConfigMode ? "true" : "false") + " }";
  
  Serial.println("Status requested: " + status);
  server.send(200, "application/json", status);
}

void handleSetTemperature() {
  if (!server.hasArg("zone") || !server.hasArg("temp")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  String zone = server.arg("zone");
  float temp = server.arg("temp").toFloat();
  
  StaticJsonDocument<512> doc;
  doc["message_type"] = "hm_get_command_queue";
  
  JsonObject message = doc.createNestedObject("message");
  message["token"] = config.api_key;
  
  JsonArray commands = message.createNestedArray("COMMANDS");
  JsonObject command = commands.createNestedObject();
  command["COMMAND"] = "{\"SET_TEMP\":{\"zone\":\"" + zone + "\",\"temp\":" + String(temp) + "}}";
  command["COMMANDID"] = millis();
  
  String messageString;
  serializeJson(doc, messageString);
  
  Serial.println("Sending set temperature command: " + messageString);
  webSocket.sendTXT(messageString);
  
  server.send(200, "text/plain", "Temperature set command sent");
}

void handleGetTemperature() {

sendCommand();
}

void handleReset() {
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

void handleGetZones() {
  StaticJsonDocument<512> doc;
  doc["message_type"] = "hm_get_command_queue";
  
  JsonObject message = doc.createNestedObject("message");
  message["token"] = config.api_key;  // Use the stored API key
  
  JsonArray commands = message.createNestedArray("COMMANDS");
  JsonObject command = commands.createNestedObject();
  command["COMMAND"] = "{'GET_ZONES': 1}";
  command["COMMANDID"] = 2;
  
  String messageString;
  serializeJson(doc, messageString);
  
  Serial.println("Sending get zones command: " + messageString);
  webSocket.sendTXT(messageString);
  
  server.send(200, "text/plain", "Get zones command sent. Check serial monitor for response.");
}

// Add a function to check WebSocket connection status
void checkWebSocketConnection() {
  if (!webSocket.isConnected()) {
    Serial.println("WebSocket not connected. Attempting to reconnect...");
    setupWebSocket();
  }
}
