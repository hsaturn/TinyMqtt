#include <TinyMqtt.h>   // https://github.com/hsaturn/TinyMqtt

/** TinyMQTT allows a disconnected mode:
  * 
  *  +-----------------------------+
  *  | ESP                         |
  *  |                 +--------+  |
  *  |       +-------->| broker |  |
  *  |       |         +--------+  |
  *  |       |             ^       |
  *  |       |             |       |
  *  |       v             v       |
  *  | +----------+  +----------+  |
  *  | | internal |  | internal |  |
  *  | | client   |  | client   |  |          
  *  | +----------+  +----------+  |          
  *  |                             |
  *  +-----------------------------+
  *
  *  In this example, local clients A and B are talking together, no need to be connected.
  *
  *  A single ESP can use this to be able to comunicate with itself with the power
  *  of MQTT, and once connected still continue to work with others.
  *
  *  The broker may still be conected if wifi is on.
  *  
  */

std::string topic="sensor/temperature";

MqttBroker broker(1883);
MqttClient mqtt_a(&broker);
MqttClient mqtt_b(&broker);

void onPublishA(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{ Serial << "--> A Received " << topic.c_str() << endl; }

void onPublishB(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{ Serial << "--> B Received " << topic.c_str() << endl; }

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial << "init" << endl;

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
	static const int intervalA = 5000;
	static uint32_t timerA = millis() + intervalA;
	
	if (millis() > timerA)
	{
		Serial << "A is publishing " << topic.c_str() << endl;
		timerA += intervalA;
		mqtt_a.publish(topic);
	}

  // ============= client B publish ================
	static const int intervalB = 3000;	// will send topic each 5000 ms
	static uint32_t timerB = millis() + intervalB;
	
	if (millis() > timerB)
	{
		Serial << "B is publishing " << topic.c_str() << endl;
		timerB += intervalB;
		mqtt_b.publish(topic);
	}
}
