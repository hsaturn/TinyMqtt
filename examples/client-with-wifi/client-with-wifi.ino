#include <TinyMqtt.h>   // https://github.com/hsaturn/TinyMqtt
#include <Streaming.h>  // https://github.com/janelia-arduino/Streaming

// TODO should be renamed to most-complete setup

/** 
  * Local broker that accept connections
  *
  * pros - Reduces internal latency (when publish is received by the same ESP)
  *      - Reduces wifi traffic
  *      - No need to have an external broker
	*      - can still report to a 'main' broker (TODO see documentation that have to be written)
	*      - accepts external clients
  *
  * cons - Takes more memory
	*      - a bit hard to understand
	*
  * This sounds crazy: a mqtt mqtt that do not need a broker !
  *  The use case arise when one ESP wants to publish topics and subscribe to them at the same time.
  *  Without broker, the ESP won't react to its own topics.
  *  
  *  TinyMqtt mqtt allows this use case to work.
  */

#include <my_credentials.h>

std::string topic="sensor/temperature";

MqttBroker broker(1883);

MqttClient mqtt_a(&broker);
MqttClient mqtt_b(&broker);

void onPublishA(const Topic& topic, const char* payload, size_t length)
{ Serial << "--> A Received " << topic.c_str() << endl; }

void onPublishB(const Topic& topic, const char* payload, size_t length)
{ Serial << "--> B Received " << topic.c_str() << endl; }

void setup()
{
  Serial.begin(115200);
	delay(500);
	Serial << "Clients with wifi " << endl;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {   Serial << '-'; delay(500); }

  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;

	broker.begin();
 
  mqtt_a.setCallback(onPublishA);
  mqtt_a.subscribe(topic);

  mqtt_b.setCallback(onPublishB);
  mqtt_b.subscribe(topic);
}

void loop()
{
	broker.loop();

  mqtt_a.loop();
  mqtt_b.loop();

  // ============= client A publish ================
	static const int intervalA = 50000;
	static uint32_t timerA = millis() + intervalA;
	
	if (millis() > timerA)
	{
		Serial << "A is publishing " << topic.c_str() << endl;
		timerA += intervalA;
		mqtt_a.publish(topic);
	}

  // ============= client B publish ================
	static const int intervalB = 30000;	// will send topic each 5000 ms
	static uint32_t timerB = millis() + intervalB;
	
	if (millis() > timerB)
	{
		Serial << "B is publishing " << topic.c_str() << endl;
		timerB += intervalB;
		mqtt_b.publish(topic, std::string(String(15+millis()%10).c_str()));
	}
}
