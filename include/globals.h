#ifndef GLOBALS_H
#define GLOBALS_H

#pragma once

#include <WebServer.h>
#include <WebSocketsClient.h>
#include <DNSServer.h>
#include <map> // Add this line

// Constants
#define EEPROM_SIZE 512
#define AP_SSID "HeatmiserSetup"
#define AP_PASSWORD "12345678"
#define DNS_PORT 53
#define CONFIG_MODE_TIMEOUT 300000         // 5 minutes in milliseconds
#define HEATMISER_PORT 4243                // Correct Heatmiser port
#define DEVICE_HOSTNAME "heatmiser-bridge" // Default hostname

// Global variables declarations
extern WebServer server;
extern WebSocketsClient webSocket;
extern DNSServer dnsServer;
extern bool isConfigMode;
extern unsigned long configModeStartTime;
extern bool endpointsCreated;
extern unsigned long lastTempRequest;
extern const unsigned long TEMP_TIMEOUT; // 2 seconds timeout
extern std::map<String, float> temperatures;

// Configuration structure
struct Config
{
    char wifi_ssid[32];
    char wifi_password[64];
    char heatmiser_ip[16];
    char api_key[64];
    bool isConfigured;
};
extern Config config;

#endif