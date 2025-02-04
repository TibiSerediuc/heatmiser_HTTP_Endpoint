#include "globals.h"
#include "dashboard.h"

void setupHttpServer()
{
  // Set up routes with error handling
  server.on("/", HTTP_GET, []() {
      try {
          handleDashboard();
      } catch (const std::exception& e) {
          server.send(500, "text/plain", "Internal server error");
      }
  });
  server.on("/api/zones", HTTP_GET, []() {
      try {
          handleGetZones();
      } catch (const std::exception& e) {
          server.send(500, "application/json", "{\"error\":\"Internal server error\"}");
      }
  });
  server.on("/api/status", HTTP_GET, []() {
      try {
          handleGetStatus();
      } catch (const std::exception& e) {
          server.send(500, "application/json", "{\"error\":\"Internal server error\"}");
      }
  });
  
  // Add handler for unknown endpoints
  server.onNotFound([]() {
      String message = "Path not found\n";
      message += "URI: " + server.uri() + "\n";
      server.send(404, "text/plain", message);
  });

  server.begin();
  Serial.println("HTTP server started");
}