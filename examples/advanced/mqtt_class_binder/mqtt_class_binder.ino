#include <TinyMqtt.h>   // https://github.com/hsaturn/TinyMqtt
#include <MqttClassBinder.h>

/**
  * Example on how to bind a class:onPublish function
  *
  * Local broker that accept connections and two local clients
  *
  *
  *  +-----------------------------+
  *  | ESP                         |
  *  |                 +--------+  | 1883 <--- External client/s
  *  |       +-------->| broker |  | 1883 <--- External client/s
  *  |       |         +--------+  |
  *  |       |             ^       |
  *  |       |             |       |
  *  |       |             |       |     -----
  *  |       v             v       |      ---
  *  | +----------+  +----------+  |       -
  *  | | internal |  | internal |  +-------*  Wifi
  *  | | client   |  | client   |  |
  *  | +----------+  +----------+  |
  *  |                             |
  *  +-----------------------------+
  *
  * pros - Reduces internal latency (when publish is received by the same ESP)
  *      - Reduces wifi traffic
  *      - No need to have an external broker
  *      - can still report to a 'main' broker (TODO see documentation that have to be written)
  *      - accepts external clients
  *      - MqttClassBinder allows to mix together many mqtt sources
  *
  * cons - Takes more memory (24 more bytes for the one MqttClassBinder<Class>
  *      - a bit hard to understand
  *
  */

const char *ssid     = "";
const char *password = "";

std::string topic_b="sensor/btemp";
std::string topic_sender= "sensor/counter";

MqttBroker broker(1883);

MqttClient mqtt_a(&broker);
MqttClient mqtt_b(&broker);
MqttClient mqtt_sender(&broker);

class MqttReceiver: public MqttClassBinder<MqttReceiver>
{
  public:

    void onPublish(const MqttClient* source, const Topic& topic, const char* payload, size_t /* length */)
    {
      Serial
        << "   * MqttReceiver received topic (" << topic.c_str() << ")"
        << " from (" << source->id() << "), "
        << " payload: (" << payload << ')' << endl;
    }
};

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial << "Clients with wifi " << endl;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial << '-'; delay(500);
  	if (strlen(ssid)==0)
	  	Serial << "****** PLEASE EDIT THE EXAMPLE AND MODIFY ssid/password *************" << endl;
  }

  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;

  broker.begin();

  MqttReceiver* receiver = new MqttReceiver;

  // receiver will receive both publication from two MqttClient
  // (that could be connected to two different brokers)
  MqttClassBinder<MqttReceiver>::onPublish(&mqtt_a, receiver);
  MqttClassBinder<MqttReceiver>::onPublish(&mqtt_b, receiver);

  mqtt_a.id("mqtt_a");
  mqtt_b.id("mqtt_b");
  mqtt_sender.id("sender");

  mqtt_a.subscribe(topic_b);
  mqtt_b.subscribe(topic_sender);

}

void loop()
{
  broker.loop();  // Don't forget to add loop for every broker and clients

  mqtt_a.loop();
  mqtt_b.loop();
  mqtt_sender.loop();

  // ============= client A publish ================
  {
    static const int interval = 5000;  // publishes every 5s (please avoid usage of delay())
    static uint32_t timer = millis() + interval;

    if (millis() > timer)
    {
      static int counter = 0;
      Serial << "Sender is publishing " << topic_sender.c_str() << endl;
      timer += interval;
      mqtt_sender.publish(topic_sender, "sent by Sender, message #"+std::string(String(counter++).c_str()));
    }
  }

  // ============= client B publish ================
  {
    static const int interval = 7000;	// will send topic each 7s
    static uint32_t timer = millis() + interval;
    static int temperature;

    if (millis() > timer)
    {
      Serial << "B is publishing " << topic_b.c_str() << endl;
      timer += interval;
      mqtt_b.publish(topic_b, "sent by B: temp="+std::string(String(16+temperature++%6).c_str()));
    }
  }
}
