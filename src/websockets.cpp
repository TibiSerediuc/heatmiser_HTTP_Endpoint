#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "websockets_commands.h"
#include "globals.h"

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
        
        // If we have a cached value, return it
        if (it != temperatures.end()) {
            String response = "{\"zone\":\"" + zoneName + 
                             "\",\"temperature\":" + 
                             String(it->second, 1) + "}";
            server.send(200, "application/json", response);
            
            // Request fresh value in background
            sendGetTemperatureCommand(zoneName);
            return;
        }
        
        // No cached value - request and wait
        sendGetTemperatureCommand(zoneName);
        lastTempRequest = millis();
        
        // Wait for response with timeout
        while (millis() - lastTempRequest < TEMP_TIMEOUT) {
            webSocket.loop(); // Keep WebSocket alive
            delay(100);
            
            // Check if temperature arrived
            it = temperatures.find(zoneName);
            if (it != temperatures.end()) {
                String response = "{\"zone\":\"" + zoneName + 
                                "\",\"temperature\":" + 
                                String(it->second, 1) + "}";
                server.send(200, "application/json", response);
                return;
            }
        }
        
        // Timeout reached
        server.send(202, "text/plain", 
            "Temperature request sent for zone: " + zoneName + 
            ". Please try again in a few seconds."); });

    Serial.println("Registered endpoint: " + getTempPath);

    Serial.println("Registered endpoints:");
    Serial.println(" - " + standbyOnPath);
    Serial.println(" - " + standbyOffPath);
  }

  Serial.println("All HTTP endpoints created");
}

void handleWebSocketMessage(uint8_t *payload, size_t length)
{
  Serial.println("Parsing message...");

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  String debugMsg;
  serializeJson(doc, debugMsg);
  Serial.println("Received JSON: " + debugMsg);

  const char *messageType = doc["message_type"];
  if (messageType)
  {
    Serial.println("Message type: " + String(messageType));

    if (strcmp(messageType, "hm_set_command_response") == 0)
    {
      int commandId = doc["command_id"];
      Serial.println("Command ID: " + String(commandId));

      // Only create endpoints for GET_ZONES response (command_id 1)
      if (commandId == 1)
      {
        String response = doc["response"];
        Serial.println("Received zones response: " + response);

        // Verify this is a zones list response by checking content
        DynamicJsonDocument zoneDoc(1024);
        DeserializationError zoneError = deserializeJson(zoneDoc, response.c_str());
        if (zoneError)
        {
          Serial.print("deserializeJson() failed for zones: ");
          Serial.println(zoneError.c_str());
          return;
        }

        // Skip if response contains "result" instead of zones
        if (zoneDoc.containsKey("result"))
        {
          Serial.println("Skipping endpoint creation - not a zones list");
          return;
        }

        // Only create endpoints if not already created
        if (!endpointsCreated)
        {
          createHttpEndpoints(zoneDoc.as<JsonObject>());
          endpointsCreated = true;
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
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("WebSocket Disconnected!");
    break;

  case WStype_CONNECTED:
    Serial.println("WebSocket Connected!");
    sendGetZonesCommand(); // Send GET_ZONES command when connected
    break;

  case WStype_TEXT:
    Serial.println("Received message: " + String((char *)payload));
    handleWebSocketMessage(payload, length);
    break;

  case WStype_ERROR:
    Serial.println("WebSocket Error!");
    if (payload)
    {
      Serial.println("Error payload: " + String((char *)payload));
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

void setupWebSocket()
{
  Serial.println("Setting up WebSocket connection...");
  Serial.println("Heatmiser IP: " + String(config.heatmiser_ip));
  Serial.println("Heatmiser Port: " + String(HEATMISER_PORT));

  // Begin WebSocket connection with SSL
  webSocket.beginSSL(config.heatmiser_ip, HEATMISER_PORT, "/");

  webSocket.onEvent(webSocketEvent);

  Serial.println("WebSocket setup completed");
}

// Helper function to extract a value after a given key.
String extractValue(const char *json, const char *key)
{
  // Locate the key in the JSON string.
  const char *keyPos = strstr(json, key);
  if (!keyPos)
    return String();

  // Find the ':' that follows the key.
  const char *colonPos = strchr(keyPos, ':');
  if (!colonPos)
    return String();

  // Skip colon and any whitespace or quotes.
  colonPos++;
  while (*colonPos == ' ' || *colonPos == '\"')
    colonPos++;

  // Now, copy the value until we hit a delimiter (comma, quote, or closing brace).
  const char *endPos = colonPos;
  while (*endPos && *endPos != ',' && *endPos != '}' && *endPos != '\"')
    endPos++;

  int len = endPos - colonPos;
  return String(colonPos).substring(0, len);
}