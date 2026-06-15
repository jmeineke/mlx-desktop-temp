#pragma once

#include <Arduino.h>
#include "door_state.h"

enum GarageConnectionState {
  GARAGE_CONNECTING,
  GARAGE_OFFLINE,
  GARAGE_READY
};

void startGarageTask();
void getGarageStatusSnapshot(Door snapshotDoors[2], bool &snapshotGarageReady, GarageConnectionState &snapshotConnectionState);
void markGarageNotReady();
