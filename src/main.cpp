#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsClient.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <uri/UriBraces.h>

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
void sendGetZonesCommand();
void createHttpEndpoints(JsonObject zones);
void sendGetLiveDataCommand(String zone);
void sendSetTemperatureCommand(String zone, float temperature);
void sendStandbyCommand(String zone, bool on);
float extractTemperatureFromRequest();
String urlEncode(const String &str);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  
  // Load configuration from EEPROM
  loadConfig();
  
  if (!config.isConfigured) {
    startConfigMode();
  } else {
    connectToNetwork();
    setupWebSocket();
    setupHttpServer();
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
      sendGetZonesCommand();  // Send GET_ZONES command when connected
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

void sendGetZonesCommand() {
  String command = "{\"message_type\":\"hm_get_command_queue\",\"message\":\"{\\\"token\\\":\\\"" + 
                   String(config.api_key) + "\\\",\\\"COMMANDS\\\":[{\\\"COMMAND\\\":\\\"{'GET_ZONES': 1}\\\",\\\"COMMANDID\\\":2}]}\"}";
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
    
    if (strcmp(messageType, "hm_set_command_response") == 0) {
      int commandId = doc["command_id"];
      Serial.println("Command ID: " + String(commandId));
      if (commandId == 2) {
        String response = doc["response"];
        Serial.println("Received zones response: " + response);
        
        DynamicJsonDocument zoneDoc(1024);
        DeserializationError zoneError = deserializeJson(zoneDoc, response.c_str());
        if (zoneError) {
          Serial.print("deserializeJson() failed for zones: ");
          Serial.println(zoneError.c_str());
          return;
        }
        createHttpEndpoints(zoneDoc.as<JsonObject>());
      }
    }
  }
}

String urlEncode(const String &str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

void createHttpEndpoints(JsonObject zones) {
  Serial.println("Creating HTTP endpoints for zones...");
  
  for(JsonPair kv : zones) {
    String zoneName = kv.key().c_str();
    String encodedZoneName = urlEncode(zoneName);
    
    // Create standby endpoints
    String standbyOnPath = "/standby_on/" + encodedZoneName;
    String standbyOffPath = "/standby_off/" + encodedZoneName;
    
    // Register handlers with explicit paths
    server.on(standbyOnPath.c_str(), HTTP_GET, [zoneName]() {
      Serial.println("Standby ON request for zone: " + zoneName);
      sendStandbyCommand(zoneName, true);
      server.send(200, "text/plain", "Standby ON sent for: " + zoneName);
    });
    
    server.on(standbyOffPath.c_str(), HTTP_GET, [zoneName]() {
      Serial.println("Standby OFF request for zone: " + zoneName);
      sendStandbyCommand(zoneName, false); 
      server.send(200, "text/plain", "Standby OFF sent for: " + zoneName);
    });

    // Add temperature endpoint
    String setTempPath = "/set_temp/" + encodedZoneName;
    server.on(setTempPath.c_str(), HTTP_GET, [zoneName]() {
      if(server.hasArg("temp")) {
        float temp = server.arg("temp").toFloat();
        Serial.println("Setting temperature for zone " + zoneName + " to " + String(temp));
        sendSetTemperatureCommand(zoneName, temp);
        server.send(200, "text/plain", "Temperature set to " + String(temp) + " for zone: " + zoneName);
      } else {
        server.send(400, "text/plain", "Missing temp parameter");
      }
    });
    
    Serial.println("Registered endpoint: " + setTempPath);
    Serial.println("Registered endpoints:");
    Serial.println(" - " + standbyOnPath);
    Serial.println(" - " + standbyOffPath);
  }
  
  Serial.println("All HTTP endpoints created");
}

void sendGetLiveDataCommand(String zone) {
  String command = "{\"message_type\":\"hm_get_command_queue\",\"message\":\"{\\\"token\\\":\\\"" + 
                   String(config.api_key) + "\\\",\\\"COMMANDS\\\":[{\\\"COMMAND\\\":\\\"{'GET_LIVE_DATA': \\\"" + zone + "\\\"}\\\",\\\"COMMANDID\\\":3}]}\"}";
  webSocket.sendTXT(command);
}

void sendSetTemperatureCommand(String zone, float temperature) {
  String command = "{\"message_type\":\"hm_get_command_queue\",\"message\":\"{\\\"token\\\":\\\"" + 
                   String(config.api_key) + "\\\",\\\"COMMANDS\\\":[{\\\"COMMAND\\\":\\\"{'SET_TEMP': [" + 
                   String(temperature, 1) + ",'" + zone + "']}\\\",\\\"COMMANDID\\\":2}]}\"}";
  Serial.println("Sending command: " + command);
  webSocket.sendTXT(command);
}

void sendStandbyCommand(String zone, bool on) {
  String command = "{\"message_type\":\"hm_get_command_queue\",\"message\":\"{\\\"token\\\":\\\"" + 
                   String(config.api_key) + "\\\",\\\"COMMANDS\\\":[{\\\"COMMAND\\\":\\\"" + 
                   (on ? "{'FROST_ON': ['" : "{'FROST_OFF': ['") + zone + "']}" + 
                   "\\\",\\\"COMMANDID\\\":3}]}\"}";
  Serial.println("Sending command: " + command);
  webSocket.sendTXT(command);
}

float extractTemperatureFromRequest() {
  if(server.hasArg("temp")){
    return server.arg("temp").toFloat();
  }
  return 0.0;  // default if not provided
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
  // Debug handler for all requests
  server.onNotFound([]() {
    String message = "No handler found\n";
    message += "URI: " + server.uri() + "\n";
    message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
    message += "Arguments: " + String(server.args()) + "\n";
    
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    
    Serial.println(message);
    server.send(404, "text/plain", message);
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
