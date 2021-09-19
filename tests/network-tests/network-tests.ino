#include <Arduino.h>
#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>

/**
  * TinyMqtt network unit tests.
	*
	* No wifi connection unit tests.
  * Checks with a local broker. Clients must connect to the local broker
	**/

using namespace std;

MqttBroker broker(1883);

std::map<std::string, std::map<Topic, int>>	published;		// map[client_id] => map[topic] = count

char* lastPayload = nullptr;
size_t lastLength;

void start_servers(int n, bool early_accept = true)
{
    ESP8266WiFiClass::resetInstances();
    ESP8266WiFiClass::earlyAccept = early_accept;
    while(n)
    {
      ESP8266WiFiClass::selectInstance(n--);
      WiFi.mode(WIFI_STA);
      WiFi.begin("fake_ssid", "fake_pwd");
    }
}

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
	if (srce)
		published[srce->id()][topic]++;
	
	if (lastPayload) free(lastPayload);
  lastPayload = strdup(payload);
	lastLength = length;
}

test(network_single_broker_begin)
{
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();

  // TODO Nothing is tested here !
}

test(network_client_to_broker_connexion)
{
  start_servers(2, true);

  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress broker_ip = WiFi.localIP();

  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(broker_ip.toString().c_str(), 1883);
  broker.loop();

  assertTrue(broker.clientsCount() == 1);
  assertTrue(client.connected());
}

test(network_one_client_one_broker_publish_and_subscribe_through_network)
{
  start_servers(2, true);
  published.clear();
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress ip_broker = WiFi.localIP();

  // We have a 2nd ESP in order to test through wifi (opposed to local)
  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(ip_broker.toString().c_str(), 1883);
  broker.loop();
  assertTrue(client.connected());

  client.setCallback(onPublish);
  client.subscribe("a/b");
  client.publish("a/b", "ab");

  for (int i =0; i<2; i++)
  {
    client.loop();
    broker.loop();
  }
  
  assertEqual(published.size(), (size_t)1);
  assertEqual((int)lastLength, (int)2); // sizeof(ab)
}

test(network_one_client_one_broker_hudge_publish_and_subscribe_through_network)
{
  start_servers(2, true);
  published.clear();
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress ip_broker = WiFi.localIP();

  // We have a 2nd ESP in order to test through wifi (opposed to local)
  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(ip_broker.toString().c_str(), 1883);
  broker.loop();
  assertTrue(client.connected());

  std::string sent;

  for(int i=0; i<200; i++)
    sent += char('0'+i%10);

  client.setCallback(onPublish);
  client.subscribe("a/b");
  client.publish("a/b", sent.c_str());

  for (int i =0; i<2; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(published.size(), (size_t)1);
  assertEqual((unsigned int)lastLength, (unsigned int)sent.size());
}

test(network_client_should_unregister_when_destroyed)
{
	assertEqual(broker.clientsCount(), (size_t)0);
	{
		MqttClient client(&broker);
		assertEqual(broker.clientsCount(), (size_t)1);
	}
	assertEqual(broker.clientsCount(), (size_t)0);
}

#if 0

// THESE TESTS ARE IN LOCAL MODE
// WE HAVE TO CONVERT THEM TO WIFI MODE (pass through virtual TCP link)

test(network_connect)
{
	assertEqual(broker.clientsCount(), (size_t)0);

	MqttClient client(&broker);
	assertTrue(client.connected());
	assertEqual(broker.clientsCount(), (size_t)1);
}

test(network_publish_should_be_dispatched)
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

test(network_publish_should_be_dispatched_to_clients)
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

test(network_unsubscribe)
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

test(network_nocallback_when_destroyed)
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

test(network_small_payload)
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

test(network_hudge_payload)
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
#endif

//----------------------------------------------------------------------------
// setup() and loop()
void setup() {
	/* delay(1000);
	Serial.begin(115200);
	while(!Serial);

	Serial.println("=============[ FAKE NETWORK TinyMqtt TESTS       ]========================");
*/
	WiFi.mode(WIFI_STA);
	WiFi.begin("network", "password");
}

void loop() {
	aunit::TestRunner::run();

	if (Serial.available()) ESP.reset();
}
