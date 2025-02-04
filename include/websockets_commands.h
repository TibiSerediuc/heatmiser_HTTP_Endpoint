#ifndef WEBSOCKETS_COMMANDS_H
#define WEBSOCKETS_COMMANDS_H
#include <Arduino.h>

void sendGetZonesCommand();
void sendSetTemperatureCommand(String zone, float temperature);
void sendStandbyCommand(String zone, bool on);
void sendGetTemperatureCommand();

#endif