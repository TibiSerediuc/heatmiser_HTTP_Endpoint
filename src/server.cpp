#include "globals.h"

void setupHttpServer()
{
  // Debug handler for all requests
  server.onNotFound([]()
  {
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