#include "TinyMqtt.h"   // https://github.com/hsaturn/TinyMqtt

const uint16_t PORT = 1883;
const uint8_t  RETAIN = 10;  // Max retained messages

MqttBroker broker(PORT, RETAIN);

/** Basic Mqtt Broker
  *
  *  +-----------------------------+
  *  | ESP                         |
  *  |       +--------+            |
  *  |       | broker |            | 1883 <--- External client/s
  *  |       +--------+            |
  *  |                             |
  *  +-----------------------------+
  *
  *  Your ESP will become a MqttBroker.
	*  You can test it with any client such as mqtt-spy for example
	*
	* Messages are retained *only* if retain > 0
	*
  */

const char* ssid = "";
const char* password = "";

void setup()
{
  Serial.begin(115200);

	if (strlen(ssid)==0)
		Console << TinyConsole::red << "****** PLEASE MODIFY ssid/password *************" << endl;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial << '.';
    delay(500);
  }
  Console << TinyConsole::green << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;

  broker.begin();
  Console << "Broker ready : " << WiFi.localIP() << " on port " << PORT << endl;
}

void loop()
{
  broker.loop();
}
