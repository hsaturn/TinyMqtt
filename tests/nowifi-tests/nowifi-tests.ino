#include <Arduino.h>
#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>

/**
  * TinyMqtt nowifi unit tests.
	*
	* No wifi connection unit tests.
  * Checks with a local broker. Clients must connect to the local broker
	**/

using namespace std;

MqttBroker broker(1883);

std::map<std::string, std::map<Topic, int>>	published;		// map[client_id] => map[topic] = count

char* lastPayload = nullptr;
size_t lastLength;

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
	if (srce)
		published[srce->id()][topic]++;
	
	if (lastPayload) free(lastPayload);
  lastPayload = strdup(payload);
	lastLength = length;
}

test(nowifi_client_should_unregister_when_destroyed)
{
	assertEqual(broker.clientsCount(), (size_t)0);
	{
		MqttClient client(&broker);
		assertEqual(broker.clientsCount(), (size_t)1);
	}
	assertEqual(broker.clientsCount(), (size_t)0);
}

test(nowifi_connect)
{
	assertEqual(broker.clientsCount(), (size_t)0);

	MqttClient client(&broker);
	assertTrue(client.connected());
	assertEqual(broker.clientsCount(), (size_t)1);
}

test(nowifi_publish_should_be_dispatched)
{
	published.clear();
	assertEqual(broker.clientsCount(), (size_t)0);

	MqttClient subscriber(&broker);
	subscriber.subscribe("a/b");
	subscriber.subscribe("a/c");
	subscriber.setCallback(onPublish);

	MqttClient publisher(&broker);
	publisher.publish("a/b");
	publisher.publish("a/c");
	publisher.publish("a/c");

	assertEqual(published.size(), (size_t)1);	// 1 client has received something
	assertEqual(published[""]["a/b"], 1);
	assertEqual(published[""]["a/c"], 2);
}

test(nowifi_publish_should_be_dispatched_to_clients)
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
	publisher.publish("a/b");		// A and B should receive this
	publisher.publish("a/c");		// A should receive this 

	assertEqual(published.size(), (size_t)2);	// 2 clients have received something
	assertEqual(published["A"]["a/b"], 1);
	assertEqual(published["A"]["a/c"], 1);
	assertEqual(published["B"]["a/b"], 1);
	assertEqual(published["B"]["a/c"], 0);
}

test(nowifi_unsubscribe)
{
	published.clear();
	assertEqual(broker.clientsCount(), (size_t)0);

	MqttClient subscriber(&broker);
	subscriber.setCallback(onPublish);
	subscriber.subscribe("a/b");

	MqttClient publisher(&broker);
	publisher.publish("a/b");		// This publish is received

	subscriber.unsubscribe("a/b");

	publisher.publish("a/b");		// Those one, no (unsubscribed)
	publisher.publish("a/b");

	assertEqual(published[""]["a/b"], 1);	// Only one publish has been received
}

test(nowifi_nocallback_when_destroyed)
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

	assertEqual(published.size(), (size_t)1);	// Only one publish has been received
}

test(nowifi_small_payload)
{
	published.clear();

	const char* payload="abcd";

	MqttClient subscriber(&broker);
	subscriber.setCallback(onPublish);
	subscriber.subscribe("a/b");

	MqttClient publisher(&broker);
	publisher.publish("a/b", payload, strlen(payload));		// This publish is received

  // coming from MqttClient::publish(...)
  assertEqual(payload, lastPayload);
	assertEqual(lastLength, (size_t)4);
}

test(nowifi_hudge_payload)
{
  const char* payload="This payload is hudge, just because its length exceeds 127. Thus when encoding length, we have to encode it on two bytes at min. This should not prevent the message from being encoded and decoded successfully !";

  MqttClient subscriber(&broker);
  subscriber.setCallback(onPublish);
  subscriber.subscribe("a/b");

	MqttClient publisher(&broker);
	publisher.publish("a/b", payload);		// This publish is received

  // onPublish should have filled lastPayload and lastLength
  assertEqual(payload, lastPayload);
	assertEqual(lastLength, strlen(payload));
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
