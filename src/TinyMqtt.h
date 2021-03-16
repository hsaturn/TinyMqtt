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

		bool matches(const Topic&) const;
};

class MqttCnx;
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
		void sendTo(MqttCnx*);
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
class MqttCnx
{
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
		MqttCnx(MqttBroker* parent, WiFiClient& client);

		~MqttCnx();

		bool connected() { return client && client->connected(); }
		void write(const char* buf, size_t length)
		{ if (client) client->write(buf, length); }

		const std::string& id() const { return clientId; }

		void loop();
		void close();
		void publish(const Topic& topic, MqttMessage& msg);

	private:
		void clientAlive();
		void processMessage();

		char flags;
		uint32_t keep_alive;
		uint32_t alive;
		bool mqtt_connected;
		WiFiClient* client;
		MqttMessage message;
		MqttBroker* parent;
		std::set<Topic>	subscriptions;
		std::string clientId;
};

class MqttClient
{
	public:
		MqttClient(IPAddress broker) : broker_ip(broker) {}

	protected:
		IPAddress broker_ip;
};

class MqttBroker
{
	public:
		MqttBroker(uint16_t port);

		void begin() { server.begin(); }
		void loop();

		bool checkUser(const char* user, uint8_t len) const
		{ return compareString(auth_user, user, len); }

		bool checkPassword(const char* password, uint8_t len) const
		{ return compareString(auth_password, password, len); }

		void publish(const Topic& topic, MqttMessage& msg);

	private:
		bool compareString(const char* good, const char* str, uint8_t str_len) const;
		std::vector<MqttCnx*>	clients;
		WiFiServer server;

		const char* auth_user = "guest";
		const char* auth_password = "guest";
};
