#include "websockets_commands.h"
#include <ArduinoJson.h>
#include "globals.h"

void sendGetZonesCommand()
{
  // Create a document for the inner message
  StaticJsonDocument<256> innerDoc;
  innerDoc["token"] = config.api_key;
  JsonArray commands = innerDoc.createNestedArray("COMMANDS");
  JsonObject cmd = commands.createNestedObject();
  // Note: Keep the inner command exactly as required by your API.
  cmd["COMMAND"] = "{'GET_ZONES': 1}";
  cmd["COMMANDID"] = 1;

  // Serialize inner JSON to a string
  String innerStr;
  serializeJson(innerDoc, innerStr);

  // Create the outer document
  StaticJsonDocument<256> outerDoc;
  outerDoc["message_type"] = "hm_get_command_queue";
  outerDoc["message"] = innerStr;

  // Serialize the entire document to a string
  String command;
  serializeJson(outerDoc, command);

  // Optional: Print to Serial for debugging purposes
  Serial.println(command);

  // Send the command over the WebSocket
  webSocket.sendTXT(command);
}

void sendGetTemperatureCommand(String zoneName)
{
  // Build the inner JSON message
  StaticJsonDocument<256> innerDoc;
  innerDoc["token"] = config.api_key; // use your stored API key

  // Create the COMMANDS array with one command object
  JsonArray commands = innerDoc.createNestedArray("COMMANDS");
  JsonObject commandObj = commands.createNestedObject();
  // Set the command string exactly as expected
  commandObj["COMMAND"] = "{'GET_LIVE_DATA':1}";
  commandObj["COMMANDID"] = 2; // adjust this ID as required

  // Serialize the inner document to a string
  String innerMessage;
  serializeJson(innerDoc, innerMessage);

  // Build the outer JSON message
  StaticJsonDocument<256> outerDoc;
  outerDoc["message_type"] = "hm_get_command_queue";
  outerDoc["message"] = innerMessage;

  // Serialize the entire outer JSON document to a string
  String fullCommand;
  serializeJson(outerDoc, fullCommand);

  // For debugging: output the command to Serial
  Serial.println(fullCommand);

  // Send the command over the WebSocket connection
  webSocket.sendTXT(fullCommand);
}

void sendSetTemperatureCommand(String zone, float temperature)
{
  // Build the inner JSON message
  StaticJsonDocument<256> innerDoc;
  innerDoc["token"] = config.api_key;

  // Create the COMMANDS array with one command object
  JsonArray commands = innerDoc.createNestedArray("COMMANDS");
  JsonObject commandObj = commands.createNestedObject();
  // Build the command string as required: {'SET_TEMP': [20.0,'Living Room']}
  String commandString = "{'SET_TEMP': [" + String(temperature, 1) + ",'" + zone + "']}";
  commandObj["COMMAND"] = commandString;
  // Set the command ID to match your Postman example (2 in this case)
  commandObj["COMMANDID"] = 2;

  // Serialize the inner JSON document to a string
  String innerMessage;
  serializeJson(innerDoc, innerMessage);

  // Build the outer JSON message
  StaticJsonDocument<256> outerDoc;
  outerDoc["message_type"] = "hm_get_command_queue";
  outerDoc["message"] = innerMessage;

  // Serialize the complete outer JSON document to a string
  String fullCommand;
  serializeJson(outerDoc, fullCommand);

  // For debugging: print the command to Serial to verify the output
  Serial.println("Sending command: " + fullCommand);

  // Send the command over the WebSocket connection
  webSocket.sendTXT(fullCommand);
}

void sendStandbyCommand(String zone, bool on)
{
  // Create the inner JSON document for the message
  StaticJsonDocument<256> innerDoc;
  innerDoc["token"] = config.api_key;

  // Create the COMMANDS array with one command object
  JsonArray commands = innerDoc.createNestedArray("COMMANDS");
  JsonObject commandObj = commands.createNestedObject();

  // Build the command string based on the 'on' parameter
  String commandStr;
  if (on)
  {
    commandStr = "{'FROST_ON':['" + zone + "']}";
  }
  else
  {
    commandStr = "{'FROST_OFF':['" + zone + "']}";
  }
  commandObj["COMMAND"] = commandStr;
  commandObj["COMMANDID"] = 4;

  // Serialize the inner document to a string
  String innerMessage;
  serializeJson(innerDoc, innerMessage);

  // Create the outer JSON document with the message type and nested inner message
  StaticJsonDocument<256> outerDoc;
  outerDoc["message_type"] = "hm_get_command_queue";
  outerDoc["message"] = innerMessage;

  // Serialize the outer document to the final command string
  String fullCommand;
  serializeJson(outerDoc, fullCommand);

  // Debug output to verify the generated command
  Serial.println("Sending command: " + fullCommand);

  // Send the command over the WebSocket connection
  webSocket.sendTXT(fullCommand);
}