#include "TinyMqtt.h"
#include <sstream>
#include <Streaming.h>

void outstring(const char* prefix, const char*p, uint16_t len)
{
	return;
	Serial << prefix << "='";
	while(len--) Serial << (char)*p++;
	Serial << '\'' << endl;
}

MqttBroker::MqttBroker(uint16_t port)
	: server(port)
{
}

MqttClient::MqttClient(MqttBroker* parent, WiFiClient& new_client)
	: parent(parent),
	mqtt_connected(false)
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
	parent->removeClient(this);
}

void MqttClient::close()
{
	if (client)
	{
		client->stop();
		delete client;
		client = nullptr;
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
			clients.erase(it);
			return;
		}
	}
	Serial << "Error cannot remove client" << endl;	// TODO should not occur
}

void MqttBroker::loop()
{
  WiFiClient client = server.available();
  
  if (client)
	{
		addClient(new MqttClient(this, client));
		Serial << "New client (" << clients.size() << ')' << endl;
	}

  for(auto it=clients.begin(); it!=clients.end(); it++)
	{
		auto client=*it;
    if(client->connected())
    {
			client->loop();
		}
		else
		{
      Serial << "Client " << client->id().c_str() << "  Disconnected" << endl;
			// Note: deleting a client not added by the broker itself will probably crash later.
			delete client;
			break;
    }
	}
}

// Should be called for inside and outside incoming publishes (all)
void MqttBroker::publish(const MqttClient* source, const Topic& topic, MqttMessage& msg)
{
	for(auto client: clients)
	{
		bool doit = false;
		if (broker && broker->connected())	// Connected: R2 R3 R5 R6
		{
			if (!client->isLocal())	// R2 go outside allowed
				doit = true;
			else // R3 any client to outside allowed
				doit = true;
		}
		else // Disconnected: R3 R4 R5
		{
			if (!source->isLocal()) // R3
			  doit = true;
			else if (client->isLocal())	// R4 local -> local
				doit = true;
		}

		if (doit) client->publish(topic, msg);	// goes outside R2
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

void MqttMessage::getString(char* &buffer, uint16_t& len)
{
	len = (buffer[0]<<8)|(buffer[1]);
	buffer+=2;
}

void MqttClient::clientAlive()
{
	if (keep_alive)
	{
		alive=millis()+1000*(keep_alive+5);
	}
	else
		alive=0;
}

void MqttClient::loop()
{
	if (alive && (millis() > alive))
	{
		Serial << "timeout client" << endl;
		close();
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
  char* payload;
  uint16_t len;
	bool bclose=true;

	switch(message.type() & 0XF0)
	{
		case MqttMessage::Type::Connect:
			if (mqtt_connected) break;
			payload = header+10;
			flags = header[7];
			keep_alive = (header[8]<<8)|(header[9]);
			if (strncmp("MQTT", header+2,4)) break;
			if (header[6]!=0x04) break;	// Level 3.1.1

			// ClientId
			message.getString(payload, len);
			clientId = std::string(payload, len);
			payload += len;

			if (flags & FlagWill)	// Will topic
			{
				message.getString(payload, len);	// Will Topic
				outstring("WillTopic", payload, len);
				payload += len;

				message.getString(payload, len);	// Will Message
				outstring("WillMessage", payload, len);
				payload += len;
			}
			if (flags & FlagUserName)
			{
				message.getString(payload, len);
				if (!parent->checkUser(payload, len)) break;
				payload += len;
			}
			if (flags & FlagPassword)
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
			message.create(MqttMessage::Type::PingResp);
			message.add(0);
			message.sendTo(this);
			bclose = false;
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
		clientAlive();
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
	for(const auto& subscription: subscriptions)
	{
		if (subscription.matches(topic))
		{
			if (client)
			{
				msg.sendTo(this);
			}
			else if (callback)
			{
				callback(topic, nullptr, 0);	// TODO 
			}
		}
	}
}

void MqttMessage::reset()
{
	curr=buffer;
	*curr=0;	// Type Unknown
	state=FixedHeader;
	size=0;
}

void MqttMessage::incoming(char in_byte)
{
	*curr++ = in_byte;
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
				vheader = curr;
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
			curr--;
			Serial << "Spurious " << _HEX(in_byte) << endl;
			state = Error;
			break;
	}
	if (curr-buffer > 250)
	{
		Serial << "Too much incoming bytes." << endl;
		curr=buffer;
	}
}

void MqttMessage::add(const char* p, size_t len)
{
	while(len--) incoming(*p);
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
	if (curr-buffer-2 >= 0)
	{
		encodeLength(buffer+1, curr-buffer-2);
		// hexdump("snd");
		client->write(buffer, curr-buffer);
	}
	else
	{
		Serial << "??? Invalid send" << endl;
		Serial << (long)end() << "-" << (long)buffer << endl;
	}
}

void MqttMessage::hexdump(const char* prefix) const
{
	if (prefix) Serial << prefix << ' ';
	Serial << (long)buffer << "-" << (long)curr << " : ";
		const char* p=buffer;
		while(p!=curr)
		{
			if (*p<16) Serial << '0';
			Serial << _HEX(*p) << ' ';
			p++;
		}
		Serial << endl;
}
