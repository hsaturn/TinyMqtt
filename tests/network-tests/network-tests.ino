// vim: ts=2 sw=2 expandtab
#include <Arduino.h>
#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>

/**
  * TinyMqtt network unit tests.
  *
  * No wifi connection unit tests.
  * Checks with a local broker. Clients must connect to the local broker
  **/

using string = TinyConsole::string;

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

std::map<string, std::map<Topic, int>>  published;    // map[client_id] => map[topic] = count

char* lastPayload = nullptr;
size_t lastLength;

void start_many_wifi_esp(int n, bool early_accept = true)
{
    ESP8266WiFiClass::resetInstances();
    ESP8266WiFiClass::earlyAccept = early_accept;
    while(n)
    {
      ESP8266WiFiClass::selectInstance(n--);
      WiFi.mode(WIFI_STA);
      WiFi.begin("fake_ssid", "fake_pwd");
    }
}

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
  if (srce)
    published[srce->id()][topic]++;

  if (lastPayload) free(lastPayload);
  lastPayload = strdup(payload);
  lastLength = length;
}

test(single_broker_begin)
{
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();

  // TODO Nothing is tested here !
}

test(suback)
{
  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress broker_ip = WiFi.localIP();

  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(broker_ip.toString().c_str(), 1883);
  broker.loop();

  assertTrue(broker.clientsCount() == 1);
  assertTrue(client.connected());

  MqttClient::counters[MqttMessage::Type::SubAck] = 0;
  client.subscribe("a/b");

  // TODO how to avoid these loops ???
  broker.loop();
  client.loop();

  assertEqual(MqttClient::counters[MqttMessage::Type::SubAck], 1);
}

test(remote_client_deletion)
{
  assertEqual(MqttClient::instances, 0);
  {
    start_many_wifi_esp(3, true);
    assertEqual(WiFi.status(), WL_CONNECTED);

    MqttBroker broker(1883);
    broker.begin();
    IPAddress broker_ip = WiFi.localIP();

    // A first remote client
    ESP8266WiFiClass::selectInstance(2);
    MqttClient remote_client;
    assertEqual(MqttClient::instances, 1);

    remote_client.connect(broker_ip.toString().c_str());
    broker.loop(); remote_client.loop();
    assertEqual(MqttClient::instances, 2);  // broker creates a client to manage remote_client

    // A second remote client
    ESP8266WiFiClass::selectInstance(3);
    MqttClient secund_client;
    assertEqual(MqttClient::instances, 3);

    secund_client.connect(broker_ip.toString().c_str());
    broker.loop(); remote_client.loop();
    assertEqual(MqttClient::instances, 4);

    // Now disconnect remote clients
    remote_client.close();
    broker.loop(); remote_client.loop();
    assertEqual(MqttClient::instances, 3);

    secund_client.close();
    broker.loop(); remote_client.loop();
    assertEqual(MqttClient::instances, 2);  // These instances are in this scope

    // Now simulate that the external client is dead without disconnecting
    secund_client.connect(broker_ip.toString().c_str());
    broker.loop(); remote_client.loop();
    assertEqual(MqttClient::instances, 3);

    WiFi.disconnect();
    broker.loop(); remote_client.loop();
    broker.loop(); remote_client.loop();
    assertEqual(MqttClient::instances, 2);

  }

  assertEqual(MqttClient::instances, 0);
}

test(broker_connect_and_client_deletion)
{
  assertEqual(MqttClient::instances, 0);
  {
    start_many_wifi_esp(2, true);
    assertEqual(WiFi.status(), WL_CONNECTED);

    MqttBroker broker(1883);
    broker.begin();

    ESP8266WiFiClass::selectInstance(2);
    MqttBroker remote_broker(1883);
    remote_broker.begin();
    IPAddress remote_broker_ip = WiFi.localIP();
    assertEqual(MqttClient::instances, 0);

    ESP8266WiFiClass::selectInstance(1);
    broker.connect(remote_broker_ip.toString().c_str());
    remote_broker.loop();
    assertEqual(remote_broker.clientsCount(), (size_t)1);

    // Here, we have two MqttClient
    // The client that connects broker to remote_broker
    // The client created by remote_broker
    assertEqual(MqttClient::instances, 2);

    broker.connect("");
    remote_broker.loop(); broker.loop();
    remote_broker.loop(); broker.loop();
    assertEqual(remote_broker.clientsCount(), (size_t)0);
  }
  assertEqual(MqttClient::instances, 0);
}

test(client_keep_alive_high)
{
  const uint32_t keep_alive=1000;
  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress broker_ip = WiFi.localIP();

  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(broker_ip.toString().c_str(), 1883, keep_alive);
  broker.loop();

  assertTrue(broker.clientsCount() == 1);
  assertTrue(client.connected());

  MqttClient::counters[MqttMessage::Type::SubAck] = 0;
  client.subscribe("a/b");

  // TODO how to avoid these loops ???
  broker.loop();
  client.loop();

  assertEqual(MqttClient::counters[MqttMessage::Type::SubAck], 1);

  uint32_t sz = broker.getClients().size();
  assertEqual(sz , (uint32_t)1);

  uint32_t ka = broker.getClients()[0]->keepAlive();
  assertEqual(ka, keep_alive);

}

test(retained_message)
{
  published.clear();

  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  broker.retain(10);
  IPAddress broker_ip = WiFi.localIP();

  MqttClient local_client(&broker, "sender");

  // Send a retained message
  // No remote client connected
  local_client.publish("topic", "retained once", true);
  for(int i=0; i<2; i++) { broker.loop(); local_client.loop(); };

  // Send a second message on the same topic (issue 86)
  local_client.publish("topic", "retained once", true);
  for(int i=0; i<2; i++) { broker.loop(); local_client.loop(); };

  // Send a second message on the same topic (issue 86)
  local_client.publish("topic", "retained last", true);
  for(int i=0; i<2; i++) { broker.loop(); local_client.loop(); };

  // No connect a client from 2nd Esp
  ESP8266WiFiClass::selectInstance(2);
  MqttClient remote_client("receiver");
  remote_client.connect(broker_ip, 1883);
  remote_client.setCallback(onPublish);

  assertTrue(remote_client.connected());
  for(int i=0; i<10; i++) { broker.loop(); local_client.loop(); remote_client.loop(); };
  assertEqual(broker.clientsCount(), (size_t) 2);

  // Should not have received anything yet
  assertEqual(published.size(), (size_t)0);

  // Now, remote client subscribes to topic
  remote_client.subscribe("#");
  for(int i=0; i<10; i++) { broker.loop(); local_client.loop(); remote_client.loop(); };

  // Check that the retained message is published once
  assertEqual(published.size(), (size_t)1);
  assertEqual(published["receiver"]["topic"], 1);

  // FIXME we should check that
  // 1 - Retained message has the retain flag set
  // 2 - Published retained messages that are send normally have their retain flag off

  // The next part of this test does not pass yet (due to remote_client.close()
  // that does not work well.
  return;

  // Now remove the retained message
  remote_client.close();
  for(int i=0; i<4; i++) { broker.loop(); local_client.loop(); remote_client.loop(); };
  assertFalse(remote_client.connected());
  assertEqual(broker.clientsCount(), (size_t) 1);

  // Disconnect / reconnect the remote clien that should receive again the message
  remote_client.connect(broker_ip, 1883);
  remote_client.subscribe("topic");
  assertTrue(remote_client.connected());
  for(int i=0; i<4; i++) { broker.loop(); local_client.loop(); remote_client.loop(); };
  assertEqual(broker.clientsCount(), (size_t) 2);
  assertEqual(published.size(), (size_t)2);

  // Remove the retained message now
  local_client.publish("topic", "", true);
  assertEqual(published.size(), (size_t)1);

  // And reconnect the remote client
  remote_client.connect(broker_ip, 1883);
  for(int i=0; i<4; i++) { broker.loop(); local_client.loop(); remote_client.loop(); };
  assertEqual(broker.clientsCount(), (size_t) 2);

  // check that the message was received
  assertEqual(published.size(), (size_t)2);
}

test(retained_payload)  // # issue #84
{
  published.clear();

  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  broker.retain(10);
  IPAddress broker_ip = WiFi.localIP();

  MqttClient local_client(&broker, "sender");

  // Send a retained message
  // No remote client connected
  local_client.publish("topic", "retained", true);
  for(int i=0; i<2; i++) { broker.loop(); local_client.loop(); };

  // No connect a client from 2nd Esp
  ESP8266WiFiClass::selectInstance(2);
  MqttClient remote_client("receiver");
  remote_client.connect(broker_ip, 1883);
  remote_client.setCallback(onPublish);
  remote_client.subscribe("#");

  assertTrue(remote_client.connected());
  for(int i=0; i<10; i++) { broker.loop(); local_client.loop(); remote_client.loop(); };

  // Check that the retained message is published
  assertEqual(lastPayload, "retained");
}

test(remote_client_disconnect_reconnect)
{
  published.clear();
  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress broker_ip = WiFi.localIP();

  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(broker_ip, 1883);

  for(int i=0; i<4; i++) { broker.loop(); client.loop(); };
  assertEqual(broker.clientsCount(), (size_t) 1);

  // Disconnect the client
  client.close();
  for(int i=0; i<4; i++) { broker.loop(); client.loop(); };
  assertEqual(broker.clientsCount(), (size_t) 0);

  // Reconnect the client
  client.connect(broker_ip, 1883);
  for(int i=0; i<4; i++) { broker.loop(); client.loop(); };
  assertEqual(broker.clientsCount(), (size_t) 1);
}

test(client_to_broker_connexion)
{
  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress broker_ip = WiFi.localIP();

  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(broker_ip.toString().c_str(), 1883);
  broker.loop();

  assertTrue(broker.clientsCount() == 1);
  assertTrue(client.connected());
}

test(one_client_one_broker_publish_and_subscribe)
{
  start_many_wifi_esp(2, true);
  published.clear();
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

  client.setCallback(onPublish);
  client.subscribe("a/b");
  client.publish("a/b", "ab");

  for (int i =0; i<2; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(published.size(), (size_t)1);
  assertEqual((int)lastLength, (int)2); // sizeof(ab)
}

test(one_client_one_broker_hudge_payload)
{
  start_many_wifi_esp(2, true);
  published.clear();
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

  std::string sent;

  for(int i=0; i<400; i++)
    sent += char('a'+i%26);

  client.setCallback(onPublish);
  client.subscribe("a/b");
  client.publish("a/b", sent.c_str());

  for (int i =0; i<2; i++)
  {
    client.loop();
    broker.loop();
  }

  assertEqual(published.size(), (size_t)1);
  assertEqual((unsigned int)lastLength, (unsigned int)sent.size());
}

test(client_should_unregister_when_destroyed)
{
  assertEqual(broker.clientsCount(), (size_t)0);
  {
    MqttClient client(&broker);
    assertEqual(broker.clientsCount(), (size_t)1);
  }
  assertEqual(broker.clientsCount(), (size_t)0);
}


// THESE TESTS ARE IN LOCAL MODE
// WE HAVE TO CONVERT THEM TO WIFI MODE (pass through virtual TCP link)

test(connect)
{
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient client(&broker);
  assertTrue(client.connected());
  assertEqual(broker.clientsCount(), (size_t)1);
}

test(publish_should_be_dispatched)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient subscriber(&broker);
  subscriber.subscribe("a/b");
  subscriber.subscribe("a/c");
  subscriber.setCallback(onPublish);

  MqttClient publisher(&broker);
  publisher.publish("a/b");
  publisher.publish("a/c");
  publisher.publish("a/c");

  assertEqual(published.size(), (size_t)1);  // 1 client has received something
  assertEqual(published[TINY_MQTT_DEFAULT_CLIENT_ID]["a/b"], 1);
  assertEqual(published[TINY_MQTT_DEFAULT_CLIENT_ID]["a/c"], 2);
}

test(publish_should_be_dispatched_to_clients)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient subscriber_a(&broker, "A");
  subscriber_a.setCallback(onPublish);
  subscriber_a.subscribe("a/b");
  subscriber_a.subscribe("a/c");

  MqttClient subscriber_b(&broker, "B");
  subscriber_b.setCallback(onPublish);
  subscriber_b.subscribe("a/b");

  MqttClient publisher(&broker);
  publisher.publish("a/b");    // A and B should receive this
  publisher.publish("a/c");    // A should receive this

  assertEqual(published.size(), (size_t)2);  // 2 clients have received something
  assertEqual(published["A"]["a/b"], 1);
  assertEqual(published["A"]["a/c"], 1);
  assertEqual(published["B"]["a/b"], 1);
  assertEqual(published["B"]["a/c"], 0);
}

test(unsubscribe)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient subscriber(&broker);
  subscriber.setCallback(onPublish);
  subscriber.subscribe("a/b");

  MqttClient publisher(&broker);
  publisher.publish("a/b");    // This publish is received

  subscriber.unsubscribe("a/b");

  publisher.publish("a/b");    // Those one, no (unsubscribed)
  publisher.publish("a/b");

  assertEqual(published[TINY_MQTT_DEFAULT_CLIENT_ID]["a/b"], 1);  // Only one publish has been received
}

test(nocallback_when_destroyed)
{
  published.clear();
  assertEqual(broker.clientsCount(), (size_t)0);

  MqttClient publisher(&broker);

  {
    MqttClient subscriber(&broker);
    subscriber.setCallback(onPublish);
    subscriber.subscribe("a/b");
    publisher.publish("a/b");
  }

  publisher.publish("a/b");

  assertEqual(published.size(), (size_t)1);  // Only one publish has been received
}

test(small_payload)
{
  published.clear();

  const char* payload="abcd";

  MqttClient subscriber(&broker);
  subscriber.setCallback(onPublish);
  subscriber.subscribe("a/b");

  MqttClient publisher(&broker);
  publisher.publish("a/b", payload, strlen(payload));    // This publish is received

  // coming from MqttClient::publish(...)
  assertEqual(payload, lastPayload);
  assertEqual(lastLength, (size_t)4);
}

test(hudge_payload)
{
  const char* payload="This payload is hudge, just because its length exceeds 127. Thus when encoding length, we have to encode it on two bytes at min. This should not prevent the message from being encoded and decoded successfully !";

  MqttClient subscriber(&broker);
  subscriber.setCallback(onPublish);
  subscriber.subscribe("a/b");          // Note -> this does not send any byte .... (nowhere to send)

  MqttClient publisher(&broker);
  publisher.publish("a/b", payload);    // This publish is received

  // onPublish should have filled lastPayload and lastLength
  assertEqual(payload, lastPayload);
  assertEqual(lastLength, strlen(payload));
  assertEqual(strcmp(payload, lastPayload), 0);
}

test(disconnected_when_broker_is_deleted)
{
  MqttBroker* broker = new MqttBroker(1883);
  broker->begin();

  MqttClient client;
  client.connect(broker);
  assertEqual(client.connected(), true);
  client.publish("a", "b");

  delete broker;
  assertEqual(client.connected(), false);
}

test(connack)
{
  const bool view = false;

  NetworkObserver check(
    [this](const WiFiClient*, const uint8_t* buffer, size_t length)
    {
      if (view) dumpMqttMessage(buffer, length);
      if (buffer[0] == MqttMessage::ConnAck)
      {
        std::string hex = bufferToHexa(buffer, length);
        assertStringCaseEqual(hex.c_str(), "20020000");
      }
    }
  );

  start_many_wifi_esp(2, true);
  assertEqual(WiFi.status(), WL_CONNECTED);

  MqttBroker broker(1883);
  broker.begin();
  IPAddress broker_ip = WiFi.localIP();

  ESP8266WiFiClass::selectInstance(2);
  MqttClient client;
  client.connect(broker_ip.toString().c_str(), 1883);
  broker.loop();

  assertTrue(broker.clientsCount() == 1);
  assertTrue(client.connected());

  MqttClient::counters[MqttMessage::Type::SubAck] = 0;
  client.subscribe("a/b");

  // TODO how to avoid these loops ???
  broker.loop();
  client.loop();

  assertEqual(MqttClient::counters[MqttMessage::Type::SubAck], 1);
}

//----------------------------------------------------------------------------
// setup() and loop()
void setup() {
  /* delay(1000);
  Serial.begin(115200);
  while(!Serial);
  */

  Serial.println("=============[ NETWORK TinyMqtt TESTS       ]========================");

  WiFi.mode(WIFI_STA);
  WiFi.begin("network", "password");
}

void loop() {
  aunit::TestRunner::run();

  if (Serial.available()) ESP.reset();
  published.clear();  // Avoid crash in unit tests due to exit handlers
}
