// vim: ts=2 sw=2 expandtab
#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>

/**
  * TinyMqtt local unit tests.
  *
  * Clients are connected to pseudo remote broker
  * The remote should be 127.0.0.1:1883 <--- But this does not work due to Esp network limitations
  * We are using 127.0.0.1 because this is simpler to test with a single ESP
  * Also, this will allow to mock and thus run Action on github
  **/

using namespace std;

MqttBroker broker(1883);

std::map<std::string, std::map<Topic, int>>  published;    // map[client_id] => map[topic] = count

const char* lastPayload;
size_t lastLength;

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
  if (srce)
    published[srce->id()][topic]++;
  lastPayload = payload;
  lastLength = length;
}

test(length_decode_greater_than_127)
{
  // TODO WRITE THIS TEST
  // The test should verify than a mqtt message with more than 127 bytes is correctly decoded
  assertEqual(1,2);
}


//----------------------------------------------------------------------------
// setup() and loop()
void setup() {
  delay(1000);
  Serial.begin(115200);
  while(!Serial);

  Serial.println("=============[ NO WIFI CONNECTION TinyMqtt TESTS ]========================");
}

void loop() {
  aunit::TestRunner::run();

  if (Serial.available()) ESP.reset();
}
