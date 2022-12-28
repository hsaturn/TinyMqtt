#include "TinyMqtt.h"    // https://github.com/hsaturn/TinyMqtt
#include "TinyStreaming.h" // https://github.com/hsaturn/TinyConsole

/** Simple Client (The simplest configuration)
  *
  *
  *                   +--------+
  *           +------>| broker |<--- <  Other client
  *           |       +--------+
  *           |
  *   +-----------------+
  *   | ESP   |         |
  *   | +----------+    |
  *   | | internal |    |
  *   | | client   |    |
  *   | +----------+    |
  *   |                 |
  *   +-----------------+
  *
	* 1 - change the ssid/password
	* 2 - change BROKER values (or keep emqx.io test broker)
	* 3 - you can use mqtt-spy to connect to the same broker and
	*     see the sensor/temperature updated by the client.
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

const char* ssid = "";
const char* password = "";

static float temp=19;
static MqttClient client;

void setup()
{
  Serial.begin(115200);
	delay(500);

	Serial << "Simple clients with wifi " << endl;
	if (strlen(ssid)==0)
		Serial << "****** PLEASE MODIFY ssid/password *************" << endl;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
	{ delay(500); Serial << '.'; }

  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;

	client.connect(BROKER, BROKER_PORT);
}

void loop()
{
	client.loop();	// Don't forget to call loop() for each broker and client

	// delay(1000);		please avoid usage of delay (see below how this done using next_send and millis())
	static auto next_send = millis();
	
	if (millis() > next_send)
	{
		next_send += 1000;

		if (not client.connected())
		{
			Serial << millis() << ": Not connected to broker" << endl;
			return;
		}

		auto rnd=random(100);

		if (rnd > 66) temp += 0.1;
		else if (rnd < 33) temp -= 0.1;

		Serial << "--> Publishing a new sensor/temperature value: " << temp << endl;

		client.publish("sensor/temperature", String(temp));
	}
}
