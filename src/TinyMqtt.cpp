#include "TinyMqtt.h"
#include <sstream>
#include <Streaming.h>

#if 1
#define debug(what) { Serial << __LINE__ << ' ' << what << endl; delay(100); }
#else
#define debug(what) {}
#endif

void outstring(const char* prefix, const char*p, uint16_t len)
{
	return;
	Serial << prefix << "='";
	while(len--) Serial << (char)*p++;
	Serial << '\'' << endl;
}

MqttBroker::MqttBroker(uint16_t port) : server(port)
{
}

MqttClient::MqttClient(MqttBroker* parent, WiFiClient& new_client)
	: parent(parent)
{
	client = new_client ? new WiFiClient(new_client) : nullptr;
	alive = millis()+5000;	// client expires after 5s if no CONNECT msg
}

MqttClient::MqttClient(MqttBroker* parent)
	: parent(parent)
{
		client = nullptr;

		parent->addClient(this);
}

MqttClient::~MqttClient()
{
	close();
	delete client;
}

void MqttClient::close()
{
	debug("close " << id().c_str());
	mqtt_connected = false;
	if (client)
	{
		client->stop();
	}

	if (parent)
	{
		parent->removeClient(this);
		parent=nullptr;
	}
}

void MqttClient::connect(std::string broker, uint16_t port)
{
	debug("cnx: closing");
	close();
	debug("cnx: closed");
	if (client) delete client;
	client = new WiFiClient;
	if (client->connect(broker.c_str(), port))
	{
	  debug("cnx: connecting");
		message.create(MqttMessage::Type::Connect);
		message.add("MQTT",4);
		message.add(0x4);	// Mqtt protocol version 3.1.1
		message.add(0x0);	// Connect flags         TODO user / name

		keep_alive = 1;
		message.add(0x00); // keep_alive
		message.add((char)keep_alive);
		message.add(clientId);
	  debug("cnx: mqtt connecting");
		message.sendTo(this);
	  debug("cnx: mqtt sent " << (int32_t)parent);
		clientAlive(0);
	}
}

void MqttBroker::addClient(MqttClient* client)
{
		clients.push_back(client);
}

void MqttBroker::removeClient(MqttClient* remove)
{
  for(auto it=clients.begin(); it!=clients.end(); it++)
	{
		auto client=*it;
		if (client==remove)
		{
			debug("Remove " << clients.size());
			clients.erase(it);
			debug("Client removed " << clients.size());
			return;
		}
	}
	debug("Error cannot remove client");	// TODO should not occur
}

void MqttBroker::loop()
{
  WiFiClient client = server.available();
  
  if (client)
	{
		addClient(new MqttClient(this, client));
		debug("New client (" << clients.size() << ')');
	}

  // for(auto it=clients.begin(); it!=clients.end(); it++)
	// use index because size can change during the loop
	for(int i=0; i<clients.size(); i++)
	{
		auto client = clients[i];
    if(client->connected())
    {
			client->loop();
		}
		else
		{
      debug("Client " << client->id().c_str() << "  Disconnected, parent=" << (int32_t)client->parent);
			// Note: deleting a client not added by the broker itself will probably crash later.
			delete client;
			break;
    }
	}
}

void MqttBroker::publish(const MqttClient* source, const Topic& topic, MqttMessage& msg)
{
	debug("publish ");
	int i=0;
	for(auto client: clients)
	{
		i++;
		Serial << "brk_" << (broker && broker->connected() ? "con" : "dis") <<
			 "	srce=" << (source->isLocal() ? "loc" : "rem") << " clt#" << i << ", local=" << client->isLocal() << ", con=" << client->connected();
		bool doit = false;
		if (broker && broker->connected())	// Connected: R2 R3 R5 R6
		{
			//    ext broker -> clients or
			// or clients -> ext broker
			if (source == broker)	// broker -> clients
				doit = true;
			else									// clients -> broker
				broker->publish(topic, msg);
		}
		else // Disconnected: R7
		{
			// All is allowed
			doit = true;
		}
		Serial << ", doit=" << doit << ' ';

		if (doit) client->publish(topic, msg);
		debug("");
	}
}

bool MqttBroker::compareString(
		const char* good,
		const char* str,
		uint8_t len) const
{
	while(len-- and *good++==*str++);

	return *good==0;
}

void MqttMessage::getString(const char* &buff, uint16_t& len)
{
	len = (buff[0]<<8)|(buff[1]);
	buff+=2;
}

void MqttClient::clientAlive(uint32_t more_seconds)
{
	if (keep_alive)
	{
		alive=millis()+1000*(keep_alive+more_seconds);
	}
	else
		alive=0;
}

void MqttClient::loop()
{
	if (alive && (millis() > alive))
	{
		if (parent)
		{
			debug("timeout client");
			close();
		}
		else
		{
			uint16_t pingreq = MqttMessage::Type::PingReq;
			client->write((uint8_t*)(&pingreq), 2);
			clientAlive(0);

			// TODO when many MqttClient passes through a local browser
			// there is no need to send one PingReq per instance.
		}
	}

	while(client && client->available()>0)
	{
		message.incoming(client->read());
		if (message.type())
		{
			processMessage();
		}
	}
}

void MqttClient::processMessage()
{
	std::string error;
	std::string s;
	// Serial << "---> INCOMING " << _HEX(message.type()) << ", mem=" << ESP.getFreeHeap() << endl;
  auto header = message.getVHeader();
  const char* payload;
  uint16_t len;
	bool bclose=true;

	switch(message.type() & 0XF0)
	{
		case MqttMessage::Type::Connect:
			if (mqtt_connected)
			{
				debug("already connected");
				break;
			}
			payload = header+10;
			mqtt_flags = header[7];
			keep_alive = (header[8]<<8)|(header[9]);
			if (strncmp("MQTT", header+2,4))
			{
				debug("bad mqtt header");
				break;
			}
			if (header[6]!=0x04)
			{
				debug("unknown level");
				break;	// Level 3.1.1
			}

			// ClientId
			message.getString(payload, len);
			debug("client id len=" << len);
			if (len>30)
			{
				Serial << '(';
				for(int i=0; i<30; i++)
				{
					if (i%5==0) Serial << ' ';
					char c=*(header+i);
					Serial << (c < 32 ? '.' : c);
				}
				Serial << " )" << endl;
				debug("Bad client id length");
				break;
			}
			clientId = std::string(payload, len);
			payload += len;

			if (mqtt_flags & FlagWill)	// Will topic
			{
				message.getString(payload, len);	// Will Topic
				outstring("WillTopic", payload, len);
				payload += len;

				message.getString(payload, len);	// Will Message
				outstring("WillMessage", payload, len);
				payload += len;
			}
			// FIXME forgetting credential is allowed (security hole)
			if (mqtt_flags & FlagUserName)
			{
				message.getString(payload, len);
				if (!parent->checkUser(payload, len)) break;
				payload += len;
			}
			if (mqtt_flags & FlagPassword)
			{
				message.getString(payload, len);
				if (!parent->checkPassword(payload, len)) break;
				payload += len;
			}

			Serial << "Connected client:" << clientId.c_str() << ", keep alive=" << keep_alive << '.' << endl;
			bclose = false;
			mqtt_connected=true;
			// Reuse received msg
			message.create(MqttMessage::Type::Connack);
			message.add(0);	// Session present (not implemented)
			message.add(0); // Connection accepted
			message.sendTo(this);
			break;

		case MqttMessage::Type::PingReq:
			if (!mqtt_connected) break;
			if (client)
			{
				uint16_t pingreq = MqttMessage::Type::PingResp;
				client->write((uint8_t*)(&pingreq), 2);
			  bclose = false;
			}
			else
			{
				debug("internal pingreq ?");
			}
			break;

		case MqttMessage::Type::Subscribe:
			if (!mqtt_connected) break;
			payload = header+2;
			message.getString(payload, len);	// Topic
			outstring("Subscribes", payload, len);

			subscribe(Topic(payload, len));
			bclose = false;
			// TODO SUBACK
			break;

		case MqttMessage::Type::Publish:
			if (!mqtt_connected) break;
			{
				uint8_t qos = message.type() & 0x6;
				payload = header;
				message.getString(payload, len);
				Topic published(payload, len);
				payload += len;
				len=message.end()-payload;
				// Serial << "Received Publish (" << published.str().c_str() << ") size=" << (int)len
				// << '(' << std::string(payload, len).c_str() << ')'	<< " msglen=" << message.length() << endl; 
				if (qos) payload+=2;	// ignore packet identifier if any
				// TODO reset DUP
				// TODO reset RETAIN
				debug("publishing to parent");
				parent->publish(this, published, message);
				// TODO should send PUBACK
				bclose = false;
			}
			break;

		case MqttMessage::Type::PubAck:
			if (!mqtt_connected) break;
			bclose = false;
			break;

		default:
			bclose=true;
			break;
	};
	if (bclose)
	{
		Serial << "*************** Error msg 0x" << _HEX(message.type());
		if (error.length()) Serial << ':' << error.c_str();
		Serial << endl;
		close();
  }
	else
	{
		clientAlive(5);
	}
	message.reset();
}

bool Topic::matches(const Topic& topic) const
{
	if (getIndex() == topic.getIndex()) return true;
	if (str() == topic.str()) return true;
	return false;
}

// publish from local client
void MqttClient::publish(const Topic& topic, const char* payload, size_t pay_length)
{
	message.create(MqttMessage::Publish);
	message.add(topic);
	message.add(payload, pay_length);
	if (parent)
		parent->publish(this, topic, message);
	else if (client)
		publish(topic, message);
	else
		Serial << "  Should not happen" << endl;
}

// republish a received publish if it matches any in subscriptions
void MqttClient::publish(const Topic& topic, MqttMessage& msg)
{
	debug("mqttclient publish " << subscriptions.size());
	for(const auto& subscription: subscriptions)
	{
		Serial << " client=" << (int32_t)client << ", topic " << topic.str().c_str() << ' ';
		if (subscription.matches(topic))
		{
			Serial << " match/send";
			if (client)
			{
				msg.sendTo(this);
			}
			else if (callback)
			{
				callback(this, topic, nullptr, 0);	// TODO 
			}
		}
		Serial << endl;
	}
}

void MqttMessage::reset()
{
	buffer.clear();
	state=FixedHeader;
	size=0;
}

void MqttMessage::incoming(char in_byte)
{
	buffer += in_byte;
	switch(state)
	{
		case FixedHeader:
			size=0;
			state = Length;
			break;
		case Length:
			size = (size<<7) + (in_byte & 0x3F);
			if (size > MaxBufferLength)
			{
				state = Error;
			}
			else if ((in_byte & 0x80) == 0)
			{
				vheader = buffer.length();
				if (size==0)
					state = Complete;
				else
					state = VariableHeader;
			}
			break;
		case VariableHeader:
		case PayLoad:
			--size;
			if (size==0)
			{
				state=Complete;
				// hexdump("rec");
			}
			break;
		case Create:
			size++;
			break;
		case Complete:
		default:
			Serial << "Spurious " << _HEX(in_byte) << endl;
			reset();
			break;
	}
	if (buffer.length() > 256)	// TODO magic 256 ?
	{
		debug("Too long");
		reset();
	}
}

void MqttMessage::add(const char* p, size_t len)
{
	incoming(len>>8);
	incoming(len & 0xFF);
	while(len--) incoming(*p++);
}

void MqttMessage::encodeLength(char* msb, int length)
{
	do
	{
		uint8_t encoded(length & 0x7F);
		length >>=7;
		if (length) encoded |= 0x80;
		*msb++ = encoded;
	} while (length);
};

void MqttMessage::sendTo(MqttClient* client)
{
	if (buffer.size()>2)
	{
		encodeLength(&buffer[1], buffer.size()-2);
		// hexdump("snd");
		client->write(&buffer[0], buffer.size());
	}
	else
	{
		Serial << "??? Invalid send" << endl;
	}
}

void MqttMessage::hexdump(const char* prefix) const
{
	if (prefix) Serial << prefix << ' ';
	Serial << "size(" << buffer.size() << ") : ";
		for(const char chr: buffer)
		{
			if (chr<16) Serial << '0';
			Serial << _HEX(chr) << ' ';
		}
		Serial << endl;
}
