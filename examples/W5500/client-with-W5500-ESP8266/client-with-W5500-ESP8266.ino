#include <SPI.h>
#include <Ethernet.h>
                        
#include "TinyMqtt.h"   // https://github.com/hsaturn/TinyMqtt

#define WIZRST D3   //Specify pin to use for reseting W5500
#define WIZCS  D1   //Specify the pin for SPI CS

#define PORT 1883
MqttBroker broker(PORT);
unsigned long Time;
unsigned long freeRam;

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
  */

#if defined(USE_ETHERNET)

byte mac[] = { 0x00, 0xAA, 0xBB, 0xE0, 0x01, 0x25 }; //MAC Address
//IPAddress ip = (192,168,0,88); //Fixed IP Address

#else

const char* ssid = "xxxxxxx";
const char* password = "xxxxxxxx";

#endif


void WizReset() {
   Serial.print("Resetting Wiz W5500 Ethernet Board...  ");
   pinMode(WIZRST, OUTPUT);
   digitalWrite(WIZRST, HIGH);
   delay(250);
   digitalWrite(WIZRST, LOW);
   delay(50);
   digitalWrite(WIZRST, HIGH);
   delay(350);
   Serial.println("Done.");
}

void setup()
{
  Serial.begin(115200);
  
#if defined(USE_ETHERNET)

  Ethernet.init(WIZCS);  // SPI CS Pin
  SPI.begin();
  WizReset();
  Console << TinyConsole::green << "Starting Ethernet...." << endl;
  Ethernet.begin(mac); //Connect using DHCP
  //Ethernet.begin(mac,ip); //Connect using Fixed IP
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Console << TinyConsole::red << "Ethernet shield was not found.  Sorry, can't run without hardware. :(" << endl;
  } else 
  if (Ethernet.linkStatus() == LinkOFF) {
    Console << TinyConsole::red << "Ethernet cable is not connected." << endl;
  }
  Console << TinyConsole::green << "Local Ethernet IP address: " << Ethernet.localIP() << endl;
  broker.begin();
  Console << "Broker ready (eth) : " << Ethernet.localIP() << " on port " << PORT << endl;

#else

  if (strlen(ssid)==0)
    Console << TinyConsole::red << "****** PLEASE MODIFY ssid/password *************" << endl;
  WiFi.disconnect();                 //Remove previous SSID & Password
  WiFi.begin(ssid, password);        // Connect to the network
  Console << TinyConsole::white << "WiFi Connecting to " << ssid << " ...";
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Console << TinyConsole::white << ".";
    i++;
    if (i > 20) break;
  }
  Console << endl << endl;
  Console << TinyConsole::green << "Connected to " << ssid << " IP address: " << WiFi.localIP() << endl;
  broker.begin();
  Console << "Broker ready (wifi) : " << WiFi.localIP() << " on port " << PORT << endl;  
    
#endif
}

void loop()
{
  broker.loop();
  if(millis()-Time>10000) {
     Time=millis();
    if(ESP.getFreeHeap()!=freeRam)
    {
      freeRam=ESP.getFreeHeap();
    Serial.print("RAM:");
    Serial.println(freeRam);
    }
  }

}
