#include <Arduino.h>
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

using string = TinyConsole::string;

MqttBroker broker(1883);

std::map<string, std::map<Topic, int>>  published;    // map[client_id] => map[topic] = count

std::string lastPayload;
size_t lastLength;

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
  if (srce)
    published[srce->id()][topic]++;
  lastPayload = std::string(payload, length);
  lastLength = length;
}

test(local_not_connected_by_default)
{
  MqttClient client;
  assertEqual(client.connected(), false);

  MqttBroker broker(1883);
  assertEqual(broker.connected(), false);
}

test(local_client_should_unregister_when_destroyed)
{
  assertEqual(broker.clientsCount(), (size_t)0);
  {
    assertEqual(broker.clientsCount(), (size_t)0);  // Ensure client is not yet connected
    MqttClient client(&broker);
    assertEqual(broker.clientsCount(), (size_t)1);  // Ensure client is now connected
  }
  assertEqual(broker.clientsCount(), (size_t)0);
}

test(local_client_do_not_disconnect_after_publishing_and_long_inactivity)
{
  published.clear();
  EpoxyTest::set_millis(0);
  MqttBroker broker(1883);
  MqttClient client(&broker, "client");
  MqttClient sender(&broker, "sender");
  broker.loop();

  client.subscribe("#");
  client.subscribe("test");
  client.setCallback(onPublish);
  assertEqual(broker.clientsCount(), (size_t)2);

  sender.publish("test", "value");
  broker.loop();

  EpoxyTest::add_seconds(600);
  client.loop();
  sender.loop();
  broker.loop();

  sender.publish("test", "value");
  broker.loop();
  sender.loop();
  broker.loop();

  assertEqual(broker.clientsCount(), (size_t)2);
  assertEqual(sender.connected(), true);
  assertEqual(client.connected(), true);

  assertEqual(published.size(), (size_t)1);  // client has received something
}

test(local_connect)
{
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient client(&broker);
  assertTrue(client.connected());
  assertEqual(broker.clientsCount(), (size_t)1);
}

test(local_publish_to_nowhere)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient publisher;
  MqttError status = publisher.publish("a/b");
  assertEqual(status, MqttError::MqttNowhereToSend);
}

test(local_publish_should_be_dispatched)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient subscriber(&broker, "");
  subscriber.subscribe("a/b");
  subscriber.subscribe("a/c");
  subscriber.setCallback(onPublish);

  MqttClient publisher(&broker);
  publisher.publish("a/b");
  publisher.publish("a/c");
  publisher.publish("a/c");

  assertEqual(published.size(), (size_t)1);  // 1 client has received something
  assertEqual(published[""]["a/b"], 1);
  assertEqual(published[""]["a/c"], 2);
}

test(hudge_payload)
{
  published.clear();
  const char* payload="This payload is hudge, just because its length exceeds 127. Thus when encoding length, we have to encode it on two bytes at min. This should not prevent the message from being encoded and decoded successfully !";

  MqttClient subscriber(&broker);
  assertEqual(broker.clientsCount(), (size_t)1);
  subscriber.setCallback(onPublish);
  subscriber.subscribe("a/b");          // Note -> this does not send any byte .... (nowhere to send)

  MqttClient publisher(&broker);
  publisher.publish("a/b", payload);    // This publish is received

  // onPublish should have filled lastPayload and lastLength
  assertEqual(payload, lastPayload.c_str());
  assertEqual(lastLength, strlen(payload));
  assertEqual(strncmp(payload, lastPayload.c_str(), lastLength), 0);
}

test(local_publish_should_be_dispatched_to_local_clients)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient subscriber_a(&broker, "A");
  subscriber_a.setCallback(onPublish);
  subscriber_a.subscribe("a/b");
  subscriber_a.subscribe("a/c");

  MqttClient subscriber_b(&broker, "B");
  subscriber_b.setCallback(onPublish);
  subscriber_b.subscribe("a/b");

  MqttClient publisher(&broker);
  publisher.publish("a/b");
  publisher.publish("a/c");

  assertEqual(published.size(), (size_t)2);  // 2 clients have received something
  assertEqual(published["A"]["a/b"], 1);
  assertEqual(published["A"]["a/c"], 1);
  assertEqual(published["B"]["a/b"], 1);
  assertEqual(published["B"]["a/c"], 0);
}

test(local_unsubscribe)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient subscriber(&broker, "");
  subscriber.setCallback(onPublish);
  subscriber.subscribe("a/b");

  MqttClient publisher(&broker);
  publisher.publish("a/b");

  subscriber.unsubscribe("a/b");

  publisher.publish("a/b");
  publisher.publish("a/b");

  assertEqual(published[""]["a/b"], 1);  // Only one publish has been received
}

test(local_nocallback_when_destroyed)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient publisher(&broker);
  {
    MqttClient subscriber(&broker);
    subscriber.setCallback(onPublish);
    subscriber.subscribe("a/b");
    publisher.publish("a/b");
  }

  publisher.publish("a/b");

  assertEqual(published.size(), (size_t)1);  // Only one publish has been received
}

//----------------------------------------------------------------------------
// setup() and loop()
void setup() {
  delay(1000);
  Serial.begin(115200);
  while(!Serial);

  Serial.println("=============[ LOCAL TinyMqtt TESTS              ]========================");
}

void loop() {
  aunit::TestRunner::run();

  if (Serial.available()) ESP.reset();
}
