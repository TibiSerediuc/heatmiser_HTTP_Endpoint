#include <WebServer.h>
#include "globals.h"

// Global variables definitions
WebServer server(80);
WebSocketsClient webSocket;
DNSServer dnsServer;
bool isConfigMode = true;
unsigned long configModeStartTime;
bool endpointsCreated = false;
unsigned long lastTempRequest = 0;
const unsigned long TEMP_TIMEOUT = 2000; // 2 seconds timeout
std::map<String, float> temperatures;
Config config;