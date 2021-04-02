#include <ESP8266WiFi.h>
#include <vector>
#include <set>
#include <string>
#include "StringIndexer.h"
#include <MqttStreaming.h>

#if 0
  #define debug(what) { Serial << __LINE__ << ' ' << what << endl; delay(100); }
  #define TINY_MQTT_DEBUG 1
#else
  #define TINY_MQTT_DEBUG 0
  #define debug(what) {}
#endif

enum MqttError
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
	const uint16_t MaxBufferLength = 255;
	public:
		enum Type
		{
			Unknown     =    0,
			Connect     = 0x10,
			ConnAck     = 0x20,
			Publish     = 0x30,
			PubAck      = 0x40,
			Subscribe   = 0x80,
			SubAck      = 0x90,
			UnSubscribe = 0xA0,
			PingReq     = 0xC0,
			PingResp    = 0xD0,
			Disconnect  = 0xE0
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
		MqttMessage(Type t, uint8_t bits_d3_d0=0) { create(t); buffer[0] |= bits_d3_d0; }
		void incoming(char byte);
		void add(char byte) { incoming(byte); }
		void add(const char* p, size_t len, bool addLength=true );
		void add(const std::string& s) { add(s.c_str(), s.length()); }
		void add(const Topic& t) { add(t.str()); }
		const char* end() const { return &buffer[0]+buffer.size(); }
		const char* getVHeader() const { return &buffer[vheader]; }
		uint16_t length() const { return buffer.size(); }

		void reset();

		// buff is MSB/LSB/STRING
		// output buff+=2, len=length(str)
		static void getString(const char* &buff, uint16_t& len);


		Type type() const
		{
			return state == Complete ? static_cast<Type>(buffer[0]) : Unknown;
		}

		void create(Type type)
		{
			buffer=(char)type;
			buffer+='\0';
			vheader=2;
			size=0;
			state=Create;
		}
		MqttError sendTo(MqttClient*);
		void hexdump(const char* prefix=nullptr) const;

	private:
		void encodeLength(char* msb, int length);

		std::string buffer;
		uint8_t vheader;
		uint16_t size;	// bytes left to receive
		State state;
};

class MqttBroker;
class MqttClient
{
	using CallBack = void (*)(const MqttClient* source, const Topic& topic, const char* payload, size_t payload_length);
	enum Flags
	{
		FlagUserName = 128,
		FlagPassword = 64,
		FlagWillRetain = 32,	// unsupported
		FlagWillQos = 16 | 8, // unsupported
		FlagWill = 4,			    // unsupported
		FlagCleanSession = 2,	// unsupported
		FlagReserved = 1
	};
	public:
		MqttClient(MqttBroker* brk = nullptr, const std::string& id="");
		MqttClient(const std::string& id) : MqttClient(nullptr, id){}

		~MqttClient();

		void connect(MqttBroker* parent);
		void connect(std::string broker, uint16_t port, uint16_t ka=10);

		bool connected() { return
			(parent!=nullptr and client==nullptr) or
			(client and client->connected()); }

		const std::string& id() const { return clientId; }
		void id(std::string& new_id) { clientId = new_id; }

		void loop();
		void close(bool bSendDisconnect=true);
		void setCallback(CallBack fun) {callback=fun; };

		// Publish from client to the world
		MqttError publish(const Topic&, const char* payload, size_t pay_length);
		MqttError publish(const Topic& t, const String& s) { return publish(t, s.c_str(), s.length()); }
		MqttError publish(const Topic& t, const std::string& s) { return publish(t,s.c_str(),s.length());}
		MqttError publish(const Topic& t) { return publish(t, nullptr, 0);};

		MqttError subscribe(Topic topic, uint8_t qos=0);
		MqttError unsubscribe(Topic topic);

		// connected to local broker
		// TODO seems to be useless
		bool isLocal() const { return client == nullptr; }

		void dump()
		{
			uint32_t ms=millis();
			Serial << "MqttClient (" << clientId.c_str() << ") " << (connected() ? " ON " : " OFF");
			Serial << ", alive=" << alive << '/' << ms << ", ka=" << keep_alive;
			Serial << (client && client->connected() ? "" : "dis") << "connected";
            message.hexdump("entrant msg");
			bool c=false;
			Serial << " [";
			for(auto s: subscriptions)
			{
				Serial << (c?", ": "")<< s.str().c_str();
				c=true;
			}

			
			Serial << "]" << endl;
		}

		static long counter;	// Number of messages sent

	private:
		MqttError sendTopic(const Topic& topic, MqttMessage::Type type, uint8_t qos);
		void resubscribe();
		void write(const char* buf, size_t length)
		{ if (client) client->write(buf, length); }

		friend class MqttBroker;
		MqttClient(MqttBroker* parent, WiFiClient& client);
		// republish a received publish if topic matches any in subscriptions
		MqttError publish(const Topic& topic, MqttMessage& msg);

		void clientAlive(uint32_t more_seconds);
		void processMessage();

		bool mqtt_connected = false;
		char mqtt_flags;
		uint32_t keep_alive;
		uint32_t alive;
		MqttMessage message;

		// TODO	having a pointer on MqttBroker may produce larger binaries
		// due to unecessary function linked if ever parent is not used
		// (this is the case when MqttBroker isn't used except here)
		MqttBroker* parent=nullptr;		// connection to local broker

		WiFiClient* client=nullptr;		// connection to mqtt client or to remote broker
		std::set<Topic>	subscriptions;
		std::string clientId;
		CallBack callback = nullptr;
};

class MqttBroker
{
	enum State
	{
		Disconnected,	// Also the initial state
		Connecting,		// connect and sends a fake publish to avoid circular cnx
		Connected,		// this->broker is connected and circular cnx avoided
	};
	public:
	  // TODO limit max number of clients
		MqttBroker(uint16_t port);
		~MqttBroker();

		void begin() { server.begin(); }
		void loop();

		uint16_t port() const { return server.port(); }

		void connect(std::string host, uint16_t port=1883);
		bool connected() const { return state == Connected; }

		size_t clientsCount() const { return clients.size(); }

		void dump()
		{
			Serial << clients.size() << " client/s" << endl;
			for(auto client: clients)
			{
				Serial << "  ";
				client->dump();
			}
		}

	private:
		friend class MqttClient;

		bool checkUser(const char* user, uint8_t len) const
		{ return compareString(auth_user, user, len); }

		bool checkPassword(const char* password, uint8_t len) const
		{ return compareString(auth_password, password, len); }


		MqttError publish(const MqttClient* source, const Topic& topic, MqttMessage& msg);

		// For clients that are added not by the broker itself
		void addClient(MqttClient* client);
		void removeClient(MqttClient* client);

		bool compareString(const char* good, const char* str, uint8_t str_len) const;
		std::vector<MqttClient*>	clients;
		WiFiServer server;

		const char* auth_user = "guest";
		const char* auth_password = "guest";
		State state = Disconnected;

		MqttClient* broker = nullptr;
};
