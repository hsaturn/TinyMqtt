// vim: ts=2 sw=2 expandtab
#include <Arduino.h>
#include <AUnit.h>
#include <TinyMqtt.h>
#include <MqttClassBinder.h>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>

// --------------------- CUT HERE - MQTT MESSAGE ROUTER FILE ----------------------------

class TestReceiver : public MqttClassBinder<TestReceiver>
{
  public:
    TestReceiver(const char* name) : MqttClassBinder(), name_(name) {}

    void onPublish(const MqttClient* /* source */, const Topic& topic, const char* payload, size_t /* length */)
    {
      Serial << "--> routed message received by " << name_ << ':' << topic.c_str() << " = " << payload << endl;
      messages[name_]++;
    }

  private:
    const std::string name_;

  public:
    static std::map<std::string, int> messages;
};

std::map<std::string, int> TestReceiver::messages;

static int unrouted = 0;
void onUnrouted(const MqttClient*, const Topic& topic, const char*, size_t)
{
  Serial << "--> unrouted: " << topic.c_str() << endl;
  unrouted++;
}


static std::string topic="sensor/temperature";

/**
  * TinyMqtt network unit tests.
  *
  * No wifi connection unit tests.
  * Checks with a local broker. Clients must connect to the local broker
  **/

// if ascii_pos = 0, no ascii dump, else ascii dump starts after column ascii_pos
std::string bufferToHexa(const uint8_t* buffer, size_t length, char sep = 0, size_t ascii_pos = 0)
{
  std::stringstream out;
  std::string ascii;
  std::string h("0123456789ABCDEF");
  for(size_t i=0; i<length; i++)
  {
    uint8_t c = buffer[i];
    out << h[ c >> 4] << h[ c & 0x0F ];
    if (sep) out << sep;
    if (ascii_pos)
    {
      if (c>=32)
        ascii += c;
      else
        ascii +='.';
    }
  }
  std::string ret(out.str());
  if (ascii_pos)
  {
    while(ret.length() < ascii_pos)
      ret += ' ';
    ret +='[' + ascii + ']';
  }
  return ret;
}

void dumpMqttMessage(const uint8_t* buffer, size_t length)
{
  std::map<int, std::string> pkt =
  { { MqttMessage::Unknown     , "Unknown     " },
    { MqttMessage::Connect     , "Connect     " },
    { MqttMessage::ConnAck     , "ConnAck     " },
    { MqttMessage::Publish     , "Publish     " },
    { MqttMessage::PubAck      , "PubAck      " },
    { MqttMessage::Subscribe   , "Subscribe   " },
    { MqttMessage::SubAck      , "SubAck      " },
    { MqttMessage::UnSubscribe , "Unsubscribe " },
    { MqttMessage::UnSuback    , "UnSubAck    " },
    { MqttMessage::PingReq     , "PingReq     " },
    { MqttMessage::PingResp    , "PingResp    " },
    { MqttMessage::Disconnect  , "Disconnect  " } };

  std::cout << "       |   data sent " << std::setw(3) << length << " : ";
  auto it = pkt.find(buffer[0] & 0xF0);
  if (it == pkt.end())
    std::cout << pkt[MqttMessage::Unknown];
  else
    std::cout << it->second;

  std::cout << bufferToHexa(buffer, length, ' ', 60) << std::endl;
}

String toString(const IPAddress& ip)
{
  return String(ip[0])+'.'+String(ip[1])+'.'+String(ip[2])+'.'+String(ip[3]);
}

MqttBroker broker(1883);

void reset_and_start_servers(int n, bool early_accept = true)
{
  MqttClassBinder<TestReceiver>::reset();
  TestReceiver::messages.clear();
  unrouted = 0;

  ESP8266WiFiClass::resetInstances();
  ESP8266WiFiClass::earlyAccept = early_accept;
  while(n)
  {
    ESP8266WiFiClass::selectInstance(n--);
    WiFi.mode(WIFI_STA);
    WiFi.begin("fake_ssid", "fake_pwd");
  }
}

test(classbind_one_client_receives_the_message)
{
  reset_and_start_servers(2, true);
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

  TestReceiver receiver("receiver");
  MqttClassBinder<TestReceiver>::onPublish(&client, &receiver);

  client.subscribe("a/b");
  client.publish("a/b", "ab");

  for (int i =0; i<10; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(TestReceiver::messages["receiver"], 1);
  assertEqual(unrouted, 0);
}

test(classbind_routes_should_be_empty_when_receiver_goes_out_of_scope)
{
  reset_and_start_servers(2, true);
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

  // Make a receiver going out of scope
  {
    TestReceiver receiver("receiver");
    MqttClassBinder<TestReceiver>::onPublish(&client, &receiver);
    assertEqual(MqttClassBinder<TestReceiver>::size(), (size_t)1);
  }

  client.subscribe("a/b");
  client.publish("a/b", "ab");

  for (int i =0; i<10; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(TestReceiver::messages["receiver"], 0);
  assertEqual(MqttClassBinder<TestReceiver>::size(), (size_t)0);
}

test(classbind_publish_should_be_dispatched_to_many_receivers)
{
  reset_and_start_servers(2, true);
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

  TestReceiver receiver_1("receiver_1");
  TestReceiver receiver_2("receiver_2");

  MqttClassBinder<TestReceiver>::onPublish(&client, &receiver_1);
  MqttClassBinder<TestReceiver>::onPublish(&client, &receiver_2);
  client.subscribe("a/b");
  client.publish("a/b", "ab");

  for (int i =0; i<10; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(TestReceiver::messages["receiver_1"], 1);
  assertEqual(TestReceiver::messages["receiver_2"], 1);
}

test(classbind_register_to_many_clients)
{
  reset_and_start_servers(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress ip_broker = WiFi.localIP();

  // We have a 2nd ESP in order to test through wifi (opposed to local)
  ESP8266WiFiClass::selectInstance(2);
  MqttClient client_1;
  client_1.connect(ip_broker.toString().c_str(), 1883);
  broker.loop();

  MqttClient client_2;
  client_2.connect(ip_broker.toString().c_str(), 1883);
  broker.loop();

  assertTrue(client_1.connected());
  assertTrue(client_2.connected());

  TestReceiver receiver("receiver");

  MqttClassBinder<TestReceiver>::onPublish(&client_1, &receiver);
  MqttClassBinder<TestReceiver>::onPublish(&client_2, &receiver);

  auto loop = [&client_1, &client_2, &broker]()
  {
    client_1.loop();
    client_2.loop();
    broker.loop();
  };

  client_1.subscribe("a/b");
  client_2.subscribe("a/b");

  // Ensure subscribptions are passed
  for (int i =0; i<5; i++) loop();

  client_1.publish("a/b", "from 1");
  client_2.publish("a/b", "from 2");

  // Ensure publishes are processed
  for (int i =0; i<5; i++) loop();

  assertEqual(TestReceiver::messages["receiver"], 4);
}

test(classbind_unrouted_fallback)
{
  reset_and_start_servers(2, true);
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

  MqttClassBinder<TestReceiver>::onUnpublished(onUnrouted);
  {
    TestReceiver receiver("receiver");
    MqttClassBinder<TestReceiver>::onPublish(&client, &receiver);
  }

  client.subscribe("a/b");
  client.publish("a/b", "from 2");

  // Ensure subscribptions are passed
  for (int i =0; i<5; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(TestReceiver::messages["receiver"], 0);
  assertEqual(unrouted, 1);
}

test(classbind_should_cleanup_when_MqttClient_dies)
{
  reset_and_start_servers(2, true);
  TestReceiver receiver("receiver");

  {
    MqttClient client;

    MqttClassBinder<TestReceiver>::onPublish(&client, &receiver);
    assertEqual(MqttClassBinder<TestReceiver>::size(), (size_t)1);
  }
  assertEqual(MqttClassBinder<TestReceiver>::size(), (size_t)1);

}

//----------------------------------------------------------------------------
// setup() and loop()
void setup() {
  /* delay(1000);
  Serial.begin(115200);
  while(!Serial);
  */

  Serial.println("=============[ FAKE NETWORK TinyMqtt TESTS       ]========================");

  WiFi.mode(WIFI_STA);
  WiFi.begin("network", "password");
}

void loop() {
  aunit::TestRunner::run();

  if (Serial.available()) ESP.reset();
}
