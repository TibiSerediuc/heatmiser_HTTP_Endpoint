#include <EEPROM.h>
#include "globals.h"
#include "config.h"
#include "websockets.h"
#include "webserver.h"
#include "network.h"
#include "websockets_commands.h"
#include "dashboard.h"

// Task handles
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t webSocketTaskHandle = NULL;
TaskHandle_t dnsServerTaskHandle = NULL;
TaskHandle_t webDashboardTaskHandle = NULL;

// Define stack sizes
const uint32_t WEBSERVER_STACK_SIZE = 8192;
const uint32_t WEBSOCKET_STACK_SIZE = 8192;
const uint32_t DNS_STACK_SIZE = 4096;

// Task priorities
const uint8_t WEBSERVER_PRIORITY = 1;
const uint8_t WEBSOCKET_PRIORITY = 2;
const uint8_t DNS_PRIORITY = 1;

// Web server task running on Core 0
void webServerTask(void *parameter) {
    while (true) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent watchdog triggers
    }
}

// WebSocket task running on Core 1
void webSocketTask(void *parameter) {
    while (true) {
        if (!isConfigMode) {
            if (!webSocket.isConnected()) {
                reconnectWebSocket();
            }
            webSocket.loop();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// DNS Server task running on Core 0
void dnsServerTask(void *parameter) {
    while (true) {
        if (isConfigMode) {
            dnsServer.processNextRequest();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void webDashboardTask(void *parameter) {
    const unsigned long UPDATE_INTERVAL = 30000;
    unsigned long lastUpdate = 0;

    while (true) {
        if (webSocket.isConnected()) {
            unsigned long currentMillis = millis();
            
            if (currentMillis - lastUpdate >= UPDATE_INTERVAL) {
                // Send single GET_LIVE_DATA command instead of one per zone
                sendGetTemperatureCommand();
                lastUpdate = currentMillis;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);

    // Load configuration from EEPROM
    loadConfig();

    if (!config.isConfigured) {
        startConfigMode();
        
        // Create DNS and Web Server tasks on Core 0
        xTaskCreatePinnedToCore(
            dnsServerTask,
            "DNSServer",
            DNS_STACK_SIZE,
            NULL,
            DNS_PRIORITY,
            &dnsServerTaskHandle,
            0
        );
        
        xTaskCreatePinnedToCore(
            webServerTask,
            "WebServer",
            WEBSERVER_STACK_SIZE,
            NULL,
            WEBSERVER_PRIORITY,
            &webServerTaskHandle,
            0
        );
    } else {
        connectToNetwork();
        setupWebSocket();
        setupHttpServer();

        // Create WebSocket task on Core 1
        xTaskCreatePinnedToCore(
            webSocketTask,
            "WebSocket",
            WEBSOCKET_STACK_SIZE,
            NULL,
            WEBSOCKET_PRIORITY,
            &webSocketTaskHandle,
            1
        );

        // Create Web Server task on Core 0
        xTaskCreatePinnedToCore(
            webServerTask,
            "WebServer",
            WEBSERVER_STACK_SIZE,
            NULL,
            WEBSERVER_PRIORITY,
            &webServerTaskHandle,
            0
        );

        // Create Dashboard task on Core 0
        xTaskCreatePinnedToCore(
            webDashboardTask,
            "Dashboard",
            WEBSERVER_STACK_SIZE,
            NULL,
            WEBSERVER_PRIORITY,
            &webDashboardTaskHandle,
            0
        );
    }
}

void loop() {
    // In RTOS, the loop function can be used for monitoring or left empty
    if (isConfigMode) {
        // Check config mode timeout
        if (millis() - configModeStartTime > CONFIG_MODE_TIMEOUT) {
            // Delete all tasks before restarting
            if (webServerTaskHandle != NULL) vTaskDelete(webServerTaskHandle);
            if (dnsServerTaskHandle != NULL) vTaskDelete(dnsServerTaskHandle);
            if (webSocketTaskHandle != NULL) vTaskDelete(webSocketTaskHandle);
            if (webDashboardTaskHandle != NULL) vTaskDelete(webDashboardTaskHandle);

            ESP.restart();
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to prevent watchdog triggers
}