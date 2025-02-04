#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "websockets_commands.h"
#include "globals.h"
#include <freertos/projdefs.h>

#define TEMP_REQUEST_TIMEOUT 5000 // Define the timeout value in milliseconds
#define TEMP_RETRY_INTERVAL 1000  // Define the retry interval in milliseconds

String urlEncode(const String &str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
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

void createHttpEndpoints(JsonObject zones)
{
  Serial.println("Creating HTTP endpoints for zones...");

  for (JsonPair kv : zones)
  {
    String zoneName = kv.key().c_str();
    if (zoneName == "result")
      continue;

    String encodedZoneName = urlEncode(zoneName);

    // Create standby endpoints
    String standbyOnPath = "/standby_on/" + encodedZoneName;
    String standbyOffPath = "/standby_off/" + encodedZoneName;

    // Register handlers with explicit paths
    server.on(standbyOnPath.c_str(), HTTP_GET, [zoneName]()
              {
      Serial.println("Standby ON request for zone: " + zoneName);
      sendStandbyCommand(zoneName, true);
      server.send(200, "text/plain", "Standby ON sent for: " + zoneName); });

    server.on(standbyOffPath.c_str(), HTTP_GET, [zoneName]()
              {
      Serial.println("Standby OFF request for zone: " + zoneName);
      sendStandbyCommand(zoneName, false); 
      server.send(200, "text/plain", "Standby OFF sent for: " + zoneName); });

    // Add temperature endpoint
    String setTempPath = "/set_temp/" + encodedZoneName;
    server.on(setTempPath.c_str(), HTTP_GET, [zoneName]()
              {
      if(server.hasArg("temp")) {
        float temp = server.arg("temp").toFloat();
        Serial.println("Setting temperature for zone " + zoneName + " to " + String(temp));
        sendSetTemperatureCommand(zoneName, temp);
        server.send(200, "text/plain", "Temperature set to " + String(temp) + " for zone: " + zoneName);
      } else {
        server.send(400, "text/plain", "Missing temp parameter");
      } });

    Serial.println("Registered endpoint: " + setTempPath);

    // Add temperature endpoint
    String getTempPath = "/get_temp/" + encodedZoneName;
    server.on(getTempPath.c_str(), HTTP_GET, [zoneName]()
              {
        auto it = temperatures.find(zoneName);
        
        // Request temperature
        sendGetTemperatureCommand(zoneName);
        lastTempRequest = millis();
        
        // Wait for response with retries
        unsigned long startWait = millis();
        while (millis() - startWait < TEMP_REQUEST_TIMEOUT) {
            webSocket.loop();
            
            // Check if temperature arrived
            it = temperatures.find(zoneName);
            if (it != temperatures.end()) {
                String response = "{\"zone\":\"" + zoneName + 
                                "\",\"temperature\":" + 
                                String(it->second, 1) + "}";
                server.send(200, "application/json", response);
                return;
            }
            
            // Small delay between checks
            vTaskDelay(pdMS_TO_TICKS(TEMP_RETRY_INTERVAL));
        }
        
        // Timeout reached
        server.send(408, "application/json", 
            "{\"error\": \"Timeout waiting for temperature from zone: " + 
            zoneName + "\"}"); });

    Serial.println("Registered endpoint: " + getTempPath);

    Serial.println("Registered endpoints:");
    Serial.println(" - " + standbyOnPath);
    Serial.println(" - " + standbyOffPath);
  }

  Serial.println("All HTTP endpoints created");
}

void handleWebSocketMessage(uint8_t *payload, size_t length)
{
  Serial.println("[WSc] Parsing message...");

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error)
  {
    Serial.print("[WSc] deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  String debugMsg;
  serializeJson(doc, debugMsg);
  Serial.println("[WSc] Parsed JSON: " + debugMsg);

  const char *messageType = doc["message_type"];
  if (!messageType)
  {
    Serial.println("[WSc] No message_type in response");
    return;
  }

  Serial.println("[WSc] Message type: " + String(messageType));

  if (strcmp(messageType, "hm_set_command_response") == 0)
  {
    if (!doc.containsKey("command_id"))
    {
      Serial.println("[WSc] No command_id in response");
      return;
    }

    int commandId = doc["command_id"];
    Serial.println("[WSc] Command ID: " + String(commandId));

    // Only create endpoints for GET_ZONES response (command_id 1)
    if (commandId == 1)
    {
      if (!doc.containsKey("response"))
      {
        Serial.println("[WSc] No response field in message");
        return;
      }

      String response = doc["response"];
      Serial.println("[WSc] Received zones response: " + response);

      DynamicJsonDocument zoneDoc(1024);
      DeserializationError zoneError = deserializeJson(zoneDoc, response.c_str());
      if (zoneError)
      {
        Serial.print("[WSc] Failed to parse zones response: ");
        Serial.println(zoneError.c_str());
        return;
      }

      if (zoneDoc.containsKey("result"))
      {
        Serial.println("[WSc] Skipping - response contains only result field");
        return;
      }

      if (!endpointsCreated)
      {
        Serial.println("[WSc] Creating HTTP endpoints...");
        createHttpEndpoints(zoneDoc.as<JsonObject>());
        endpointsCreated = true;
        Serial.println("[WSc] Endpoints created successfully");
      }
      else
      {
        Serial.println("[WSc] Endpoints already exist");
      }
    }
    // Handle GET_LIVE_DATA response (command_id 2)
    else if (commandId == 2)
    {

      String response = doc["response"];
      Serial.println("Free heap before parsing: " + String(ESP.getFreeHeap()));

      // Create filter to only parse needed fields
      StaticJsonDocument<128> filter;
      filter["devices"][0]["ZONE_NAME"] = true;
      filter["devices"][0]["ACTUAL_TEMP"] = true;

      // Use larger buffer with filter
      DynamicJsonDocument responseDoc(32768); // Increased buffer
      DeserializationError responseError = deserializeJson(
          responseDoc,
          response,
          DeserializationOption::Filter(filter));

      if (responseError)
      {
        Serial.print("Failed to parse LIVE_DATA response: ");
        Serial.println(responseError.c_str());
        return;
      }

      JsonArray devices = responseDoc["devices"];
      if (!devices.isNull())
      {
        for (JsonObject device : devices)
        {
          if (device.containsKey("ZONE_NAME") && device.containsKey("ACTUAL_TEMP"))
          {
            String zoneName = device["ZONE_NAME"].as<String>();
            String tempStr = device["ACTUAL_TEMP"].as<String>();
            temperatures[zoneName] = tempStr.toFloat();
            Serial.println("Temperature for " + zoneName + ": " + tempStr);
          }
        }
      }

      Serial.println("Free heap after parsing: " + String(ESP.getFreeHeap()));
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("[WSc] Disconnected!");
    endpointsCreated = false; // Reset endpoints flag on disconnect
    break;

  case WStype_CONNECTED:
    Serial.println("[WSc] Connected to Heatmiser!");
    Serial.println("[WSc] Requesting zones...");
    sendGetZonesCommand();
    break;

  case WStype_TEXT:
    Serial.println("[WSc] Received: " + String((char *)payload));
    handleWebSocketMessage(payload, length);
    break;

  case WStype_ERROR:
    Serial.println("[WSc] Error occurred!");
    if (payload)
    {
      Serial.println("[WSc] Error payload: " + String((char *)payload));
    }
    break;

  case WStype_BIN:
    Serial.println("[WSc] Got binary length: " + String(length));
    break;

  case WStype_PING:
    Serial.println("[WSc] Got ping");
    break;

  case WStype_PONG:
    Serial.println("[WSc] Got pong");
    break;
  }
}

void setupWebSocket()
{
  Serial.println("[WSc] Setting up WebSocket connection...");
  Serial.println("[WSc] Heatmiser IP: " + String(config.heatmiser_ip));
  Serial.println("[WSc] Heatmiser Port: " + String(HEATMISER_PORT));

  // Begin WebSocket connection with SSL
  webSocket.beginSSL(config.heatmiser_ip, HEATMISER_PORT, "/");
  webSocket.onEvent(webSocketEvent);

  Serial.println("[WSc] Setup completed, waiting for connection...");
}
