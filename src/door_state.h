#pragma once

#include <Arduino.h>

struct Door {
  bool   open;
  bool   lastOpen;
  String name;
  String lastName;
};
