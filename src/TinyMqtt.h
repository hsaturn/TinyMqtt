// vim: ts=2 sw=2 expandtab
#pragma once

#ifndef TINY_MQTT_DEBUG
#define TINY_MQTT_DEBUG 0
#endif
#ifndef TINY_MQTT_DEFAULT_ALIVE
#define TINY_MQTT_DEFAULT_ALIVE 10
#endif
#define TINY_MQTT_CLIENT_ALIVE_TOLERANCE 5

// TODO Should add a AUnit with both TINY_MQTT_ASYNC and not TINY_MQTT_ASYNC
// #define TINY_MQTT_ASYNC  // Uncomment this to use ESPAsyncTCP instead of normal cnx

#if defined(ESP8266) || defined(EPOXY_DUINO)
  #ifdef TINY_MQTT_ASYNC
    #include <ESPAsyncTCP.h>
  #else
    #include <ESP8266WiFi.h>
  #endif
#elif defined(ESP32)
  #include <WiFi.h>
  #ifdef TINY_MQTT_ASYNC
    #include <AsyncTCP.h> // https://github.com/me-no-dev/AsyncTCP
  #endif
#endif
#ifdef EPOXY_DUINO
  #define dbg_ptr uint64_t
#else
  #define dbg_ptr uint32_t
#endif

#ifdef WIO_TERMINAL
  // Uncommon board handling
  // If you have a problem with this line, just remove it.
  // Note: https://github.com/hsaturn/TinyMqtt/issues/41
  #include <rpcWiFi.h>
#endif

#include <memory>
#include <set>
#include <string>
#include "StringIndexer.h"

#define TINY_MQTT_DEFAULT_CLIENT_ID "Tiny"

#include <TinyStreaming.h>
#if TINY_MQTT_DEBUG
  #include <TinyConsole.h>    // https://github.com/hsaturn/TinyConsole
  struct TinyMqtt
  {
    static int debug;
  };

  #define debug(what) { if (TinyMqtt::debug>=1) Console << (int)__LINE__ << ' ' << what << TinyConsole::white << endl; delay(10); }
#else
  #define debug(what) {}
#endif

#ifdef TINY_MQTT_ASYNC
  using TcpClient = AsyncClient;
  using TcpServer = AsyncServer;
#else
  using TcpClient = WiFiClient;
  using TcpServer = WiFiServer;
#endif

enum __attribute__((packed)) MqttError
{
  MqttOk = 0,
  MqttNowhereToSend=1,
  MqttInvalidMessage=2,
};

class Topic : public IndexedString
{
  public:
    Topic(const char* s, uint8_t len) : IndexedString(s,len){}
    Topic(const char* s) : Topic(s, strlen(s)) {}
    Topic(const std::string s) : Topic(s.c_str(), s.length()){};

    const char* c_str() const { return str().c_str(); }

    bool matches(const Topic&) const;
};

class MqttClient;
class MqttMessage
{
  const uint16_t MaxBufferLength = 4096;  //hard limit: 16k due to size decoding
  public:
    enum __attribute__((packed)) Type
    {
      Unknown     =    0,
      Connect     = 0x10,
      ConnAck     = 0x20,
      Publish     = 0x30,
      PubAck      = 0x40,
      Subscribe   = 0x80,
      SubAck      = 0x90,
      UnSubscribe = 0xA0,
      UnSuback    = 0xB0,
      PingReq     = 0xC0,
      PingResp    = 0xD0,
      Disconnect  = 0xE0
    };

    enum __attribute__((packed)) State
    {
      FixedHeader=0,
      Length=1,
      VariableHeader=2,
      PayLoad=3,
      Complete=4,
      Error=5,
      Create=6
    };

    static inline uint32_t getSize(const char* buffer)
    {
      const unsigned char* bun = (const unsigned char*)buffer;
      return (*bun << 8) | bun[1]; }

    MqttMessage() { reset(); }
    MqttMessage(Type t, uint8_t bits_d3_d0=0) { create(t); buffer[0] |= bits_d3_d0; }
    void incoming(char byte);
    void add(char byte) { incoming(byte); }
    void add(const char* p, size_t len, bool addLength=true );
    void add(const std::string& s) { add(s.c_str(), s.length()); }
    void add(const Topic& t) { add(t.str()); }
    const char* end() const { return &buffer[0]+buffer.size(); }
    const char* getVHeader() const { return &buffer[vheader]; }
    void complete() { encodeLength(); }

    void reset();

    // buff is MSB/LSB/STRING
    // output buff+=2, len=length(str)
    static void getString(const char* &buff, uint16_t& len);

    Type type() const
    {
      return state == Complete ? static_cast<Type>(buffer[0] & 0xF0) : Unknown;
    }

    uint8_t flags() const { return static_cast<uint8_t>(buffer[0] & 0x0F); }

    void create(Type type)
    {
      buffer=(decltype(buffer)::value_type)type;
      buffer+='\0';    // reserved for msg length byte 1/2
      buffer+='\0';    // reserved for msg length byte 2/2 (fixed)
      vheader=3;      // Should never change
      size=0;
      state=Create;
    }
    MqttError sendTo(MqttClient*);
    void hexdump(const char* prefix=nullptr) const;

  private:
    void encodeLength();

    std::string buffer;
    uint8_t vheader;
    uint16_t size;  // bytes left to receive
    State state;
};

class MqttBroker;
class MqttClient
{
  enum __attribute__((packed)) Flags
  {
    FlagUserName = 128,
    FlagPassword = 64,
    FlagWillRetain = 32,   // unsupported
    FlagWillQos = 16 | 8,  // unsupported
    FlagWill = 4,          // unsupported
    FlagCleanSession = 2,  // unsupported

    FlagReserved = 1,      // use reserved as connected (save 1 byte)
    FlagConnected = 1
  };
  public:

    using CallBack = void (*)(const MqttClient* source, const Topic& topic, const char* payload, size_t payload_length);

    /** Constructor. Broker is the adress of a local broker if not null
        If you want to connect elsewhere, leave broker null and use connect() **/
    MqttClient(MqttBroker* broker = nullptr, const std::string& id = TINY_MQTT_DEFAULT_CLIENT_ID);
    MqttClient(const std::string& id) : MqttClient(nullptr, id){}

    ~MqttClient();

    void connect(MqttBroker* local_broker);
    void connect(std::string broker, uint16_t port, uint16_t keep_alive = TINY_MQTT_DEFAULT_ALIVE);

    // TODO it seems that connected returns true in tcp mode even if
    // no negociation occurred
    bool connected()
    {
      return (local_broker!=nullptr and tcp_client==nullptr)
           or (tcp_client and tcp_client->connected());
    }

    void write(const char* buf, size_t length)
    {
      if (tcp_client) tcp_client->write(buf, length);
    }

    const std::string& id() const { return clientId; }
    void id(const std::string& new_id) { clientId = new_id; }

    /** Should be called in main loop() */
    void loop();
    void close(bool bSendDisconnect=true);
    void setCallback(CallBack fun)
    {
      callback=fun;
      #if TINY_MQTT_DEBUG
        Console << TinyConsole::magenta << "Callback set to " << (long)fun << TinyConsole::white << endl;
        if (callback) callback(this, "test/topic", "value", 5);
      #endif
    };

    // Publish from client to the world
    MqttError publish(const Topic&, const char* payload, size_t pay_length);
    MqttError publish(const Topic& t, const char* payload) { return publish(t, payload, strlen(payload)); }
    MqttError publish(const Topic& t, const String& s) { return publish(t, s.c_str(), s.length()); }
    MqttError publish(const Topic& t, const std::string& s) { return publish(t,s.c_str(),s.length());}
    MqttError publish(const Topic& t) { return publish(t, nullptr, 0);};

    MqttError subscribe(Topic topic, uint8_t qos=0);
    MqttError unsubscribe(Topic topic);
    bool isSubscribedTo(const Topic& topic) const;

    // connected to local broker
    // TODO seems to be useless
    bool isLocal() const { return tcp_client == nullptr; }

    void dump(std::string indent="")
    {
      (void)indent;
      #if TINY_MQTT_DEBUG
        uint32_t ms=millis();
        Console << indent << "+-- " << '\'' << clientId.c_str() << "' " << (connected() ? " ON " : " OFF");
        Console << ", alive=" << alive << '/' << ms << ", ka=" << keep_alive << ' ';
        if (tcp_client)
        {
          if (tcp_client->connected())
            Console << TinyConsole::green << "connected";
          else
            Console << TinyConsole::red << "disconnected";
          Console << TinyConsole::white;
        }
        if (subscriptions.size())
        {
          bool c = false;
          Console << " [";
          for(auto s: subscriptions)
          {
            if (c) Console << ", ";
            Console << s.str().c_str();
            c=true;
          }
          Console << ']';
        }
        Console << TinyConsole::erase_to_end << endl;
      #endif
    }

#ifdef EPOXY_DUINO
    static std::map<MqttMessage::Type, int> counters;  // Number of processed messages
#endif
    uint32_t keepAlive() const { return keep_alive; }

  private:

    // event when tcp/ip link established (real or fake)
    static void onConnect(void * client_ptr, TcpClient*);
#ifdef TINY_MQTT_ASYNC
    static void onData(void* client_ptr, TcpClient*, void* data, size_t len);
#endif
    MqttError sendTopic(const Topic& topic, MqttMessage::Type type, uint8_t qos);
    void resubscribe();

    friend class MqttBroker;
    MqttClient(MqttBroker* local_broker, TcpClient* client);
    // republish a received publish if topic matches any in subscriptions
    MqttError publishIfSubscribed(const Topic& topic, MqttMessage& msg);

    void clientAlive();
    void processMessage(MqttMessage* message);

    char mqtt_flags = 0;
    uint16_t keep_alive = 30;
    // for client connected to remote broker, PingReq is sent when millis() >= alive
    // for a client managed by a broker, disconnect it if millis() >= alive
    uint32_t alive; // PingReq if millis() > alive,
    MqttMessage message;

    // connection to local broker, or link to the parent
    // when MqttBroker uses MqttClient for each external connexion
    MqttBroker* local_broker=nullptr;

    std::unique_ptr<TcpClient> tcp_client;    // connection to remote broker
    std::set<Topic> subscriptions;
    std::string clientId;
    CallBack callback = nullptr;
};

class MqttBroker
{
  enum __attribute__((packed)) State
  {
    Disconnected,  // Also the initial state
    Connecting,    // connect and sends a fake publish to avoid circular cnx
    Connected,    // this->broker is connected and circular cnx avoided
  };
  public:
    // TODO limit max number of clients
    MqttBroker(uint16_t port);

    void begin() { server->begin(); }
    void loop();

    void connect(const std::string& host, uint16_t port=1883);
    bool connected() const { return state == Connected; }


    void dump(std::string indent="")
    {
      for(const auto& client: clients)
        client->dump(indent);
    }

    using Clients = std::set<std::unique_ptr<MqttClient>>;
    using LocalClients = std::set<MqttClient*>;

    const Clients& getClients() const { return clients; }
    const LocalClients& getLocalClients() const { return local_clients; }

    size_t clientsCount() const { return clients.size(); }
    size_t localClientsCount() const { return local_clients.size(); }

  private:
    friend class MqttClient;

    static void onClient(void*, TcpClient*);
    bool checkUser(const char* user, uint8_t len) const
    { return compareString(auth_user, user, len); }

    bool checkPassword(const char* password, uint8_t len) const
    { return compareString(auth_password, password, len); }


    MqttError publish(const MqttClient* source, const Topic& topic, MqttMessage& msg) const;

    MqttError subscribe(const Topic& topic, uint8_t qos);

    void addClient(MqttClient* local) { local_clients.insert(local); }
    void addClient(TcpClient* client);

    void removeClient(MqttClient* local);

    bool compareString(const char* good, const char* str, uint8_t str_len) const;
    Clients clients;
    LocalClients local_clients;

  private:
    std::unique_ptr<TcpServer> server;

    const char* auth_user = "guest";
    const char* auth_password = "guest";
    MqttClient* remote_broker = nullptr;

    State state = Disconnected;
};
