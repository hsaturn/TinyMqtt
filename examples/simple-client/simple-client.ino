#include <ESP8266WiFi.h>
#include "TinyMqtt.h"   // https://github.com/hsaturn/TinyMqtt

/** Simple Client
  *
	* This is the simplest Mqtt client configuration
	*
	* pro  - small memory footprint (both ram and flash)
	*      - very simple to setup and use
	*
	* cons - cannot work without wifi connection
  *      - stop working if broker is down
	*      - local publishes takes more time (because they go outside)
	*/

#include <my_credentials.h>

static float temp=19;
static MqttClient client;

void setup() 
{
  Serial.begin(115200);
	delay(500);
	Serial << "Simple clients with wifi " << endl;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED)
	{ delay(500); Serial << '.'; }

  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;  

	client.connect("192.168.1.40", 1883);	// Put here your broker ip / port

}

void loop()
{
	client.loop();

	delay(1000);

	temp += (random(100)>50 ? 0.1 : -0.1);
	client.publish("sensor/temperature", String(temp));

}
