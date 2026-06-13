#pragma once

#include <Arduino.h>
#include "door_state.h"

extern Door doors[2];
extern volatile bool garageReady;

void startGarageTask();
void markGarageNotReady();
