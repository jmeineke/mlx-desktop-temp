#pragma once

#include <Arduino.h>
#include "door_state.h"

enum GarageConnectionState {
  GARAGE_CONNECTING,
  GARAGE_OFFLINE,
  GARAGE_READY
};

extern Door doors[2];
extern volatile bool garageReady;
extern volatile GarageConnectionState garageConnectionState;

void startGarageTask();
void markGarageNotReady();
