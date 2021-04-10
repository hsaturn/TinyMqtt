#include "TinyMqtt.h"   // https://github.com/hsaturn/TinyMqtt

/** Simple Client
  *
	* This is the simplest Mqtt client configuration
	*
	* 1 - edit my_credentials.h to setup wifi essid/password
	* 2 - change BROKER values (or keep emqx.io test broker)
	*
	* pro  - small memory footprint (both ram and flash)
	*      - very simple to setup and use
	*
	* cons - cannot work without wifi connection
  *      - stop working if broker is down
	*      - local publishes takes more time (because they go outside)
	*/

const char* BROKER = "broker.emqx.io";
const uint16_t BROKER_PORT = 1883;

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

	client.connect(BROKER, BROKER_PORT);	// Put here your broker ip / port
}

void loop()
{
	client.loop();

	delay(1000);

	auto rnd=random(100);

	if (rnd > 66) temp += 0.1;
	else if (rnd < 33) temp -= 0.1;

	client.publish("sensor/temperature", String(temp));

}
