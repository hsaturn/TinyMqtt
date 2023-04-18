#include <Arduino.h>
#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

// Only compilation check, so do nothing

void setup() {}
void loop() {
  aunit::TestRunner::run();
}
