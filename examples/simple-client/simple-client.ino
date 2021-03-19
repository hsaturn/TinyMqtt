#include <ESP8266WiFi.h>
#include "TinyMqtt.h"   // https://github.com/hsaturn/TinyMqtt
#include <Streaming.h>  // https://github.com/janelia-arduino/Streaming

const char *ssid = ;      // Put here your wifi SSID ("ssid")
const char *password = ;  // Put here your Wifi password ("pwd")

#define PORT 1883
MqttBroker broker(PORT);

void setup() 
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {   
    delay(500);
    Serial << '.';
    delay(500);
  }
  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;  

  broker.begin();
  Serial << "Broker ready : " << WiFi.localIP() << " on port " << PORT << endl;
}

void loop()
{
  broker.loop();
}
