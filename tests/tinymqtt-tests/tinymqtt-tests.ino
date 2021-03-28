#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>

/**
  * TinyMqtt local unit tests.
	*
	* No wifi connection unit tests.
	**/

using namespace std;

MqttBroker broker(1883);

std::map<std::string, std::map<Topic, int>>	published;		// map[client_id] => map[topic] = count

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
	if (srce)
		published[srce->id()][topic]++;
}

test(local_client_should_unregister_when_destroyed)
{
	assertEqual(broker.clientsCount(), (size_t)0);
	{
		MqttClient client(&broker);
		assertEqual(broker.clientsCount(), (size_t)1);
	}
	assertEqual(broker.clientsCount(), (size_t)0);
}

test(local_connect)
{
	assertEqual(broker.clientsCount(), (size_t)0);

	MqttClient client(&broker);
	assertTrue(client.connected());
	assertEqual(broker.clientsCount(), (size_t)1);
}

test(local_publish_should_be_dispatched)
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
	assertTrue(published[""]["a/b"] == 1);
	assertTrue(published[""]["a/c"] == 2);
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

	assertEqual(published.size(), (size_t)2);	// 2 clients have received something
	assertTrue(published["A"]["a/b"] == 1);
	assertTrue(published["A"]["a/c"] == 1);
	assertTrue(published["B"]["a/b"] == 1);
	assertTrue(published["B"]["a/c"] == 0);
}

test(local_unsubscribe)
{
	published.clear();
	assertEqual(broker.clientsCount(), (size_t)0);

	MqttClient subscriber(&broker);
	subscriber.setCallback(onPublish);
	subscriber.subscribe("a/b");

	MqttClient publisher(&broker);
	publisher.publish("a/b");

	// subscriber.unsubscribe("a/b");		TODO not yet implemented

	publisher.publish("a/b");

	assertTrue(published[""]["a/b"] == 1);	// Only one publish has been received
}

test(local_nocallback_when_destroyed)
{
	published.clear();
	assertEqual(broker.clientsCount(), (size_t)0);

	{
		MqttClient subscriber(&broker);
		subscriber.setCallback(onPublish);
		subscriber.subscribe("a/b");
	}

	MqttClient publisher(&broker);
	publisher.publish("a/b");

	assertEqual(published.size(), (size_t)0);	// Only one publish has been received
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
