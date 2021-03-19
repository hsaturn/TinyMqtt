#include <ESP8266WiFi.h>
#include <vector>
#include <set>
#include <string>
#include "StringIndexer.h"

#define MaxBufferLength 255

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
	public:
		enum Type
		{
			Unknown   =    0,
			Connect   = 0x10,
			Connack   = 0x20,
			Publish   = 0x30,
			PubAck    = 0x40,
			Subscribe = 0x80,
			PingReq   = 0xC0,
			PingResp  = 0xD0,
		};
		enum State
		{
			FixedHeader=0,
			Length=1,
			VariableHeader=2,
			PayLoad=3,
			Complete=4,
			Error=5,
			Create=6
		};

		MqttMessage() { reset(); }
		MqttMessage(Type t) { create(t); }
		void incoming(char byte);
		void add(char byte) { incoming(byte); }
		void add(const char* p, size_t len);
		void add(const std::string& s) { add(s.c_str(), s.length()); }
		void add(const Topic& t) { add(t.str()); }
		char* getVHeader() const { return vheader; }
		char* end() const { return curr; }
		uint16_t length() const { return curr-buffer; }

		void reset();

		// buff is MSB/LSB/STRING
		// output buff+=2, len=length(str)
		void getString(char* &buff, uint16_t& len);


		Type type() const
		{
			return state == Complete ? static_cast<Type>(buffer[0]) : Unknown;
		}

		void create(Type type)
		{
			buffer[0]=type;
			curr=buffer+2;
			vheader=curr;
			size=0;
			state=Create;
		}
		void sendTo(MqttClient*);
		void hexdump(const char* prefix=nullptr) const;

	private:
		void encodeLength(char* msb, int length);

	  char buffer[256];	// TODO 256 ?
		char* vheader;
		char* curr;
		uint16_t size;	// bytes left to receive
		State state;
};

class MqttBroker;
class MqttClient
{
	using CallBack = void (*)(const Topic& topic, const char* payload, size_t payload_length);
	enum Flags
	{
		FlagUserName = 128,
		FlagPassword = 64,
		FlagWillRetain = 32,	// unsupported
		FlagWillQos = 16 | 8, // unsupported
		FlagWill = 4,			// unsupported
		FlagCleanSession = 2,	// unsupported
		FlagReserved = 1
	};
	public:
		MqttClient(MqttBroker*);

		~MqttClient();

		bool connected() { return client==nullptr || client->connected(); }
		void write(const char* buf, size_t length)
		{ if (client) client->write(buf, length); }

		const std::string& id() const { return clientId; }

		void loop();
		void close();
		void setCallback(CallBack fun) {callback=fun; };

		// Publish from client to the world
		void publish(const Topic&, const char* payload, size_t pay_length);
		void publish(const Topic& t, const std::string& s) { publish(t,s.c_str(),s.length());}
		void publish(const Topic& t) { publish(t, nullptr, 0);};

		void subscribe(Topic topic) { subscriptions.insert(topic); }
		void unsubscribe(Topic& topic);

	private:
		friend class MqttBroker;
		MqttClient(MqttBroker* parent, WiFiClient& client);
		// republish a received publish if topic matches any in subscriptions
		void publish(const Topic& topic, MqttMessage& msg);

		void clientAlive();
		void processMessage();

		char flags;
		uint32_t keep_alive;
		uint32_t alive;
		bool mqtt_connected;
		WiFiClient* client;		// nullptr if this client is local
		MqttMessage message;
		MqttBroker* parent;
		std::set<Topic>	subscriptions;
		std::string clientId;
		CallBack callback;
};

class MqttBroker
{
	public:
		MqttBroker(uint16_t port);

		void begin() { server.begin(); }
		void loop();

		uint8_t port() const { return server.port(); }

	private:
		friend class MqttClient;

		bool checkUser(const char* user, uint8_t len) const
		{ return compareString(auth_user, user, len); }

		bool checkPassword(const char* password, uint8_t len) const
		{ return compareString(auth_password, password, len); }


		void publish(const Topic& topic, MqttMessage& msg);

		// For clients that are added not by the broker itself
		void addClient(MqttClient* client);
		void removeClient(MqttClient* client);

		bool compareString(const char* good, const char* str, uint8_t str_len) const;
		std::vector<MqttClient*>	clients;
		WiFiServer server;

		const char* auth_user = "guest";
		const char* auth_password = "guest";
};
