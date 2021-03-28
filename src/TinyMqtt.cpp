#include "TinyMqtt.h"
#include <sstream>

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

MqttBroker::~MqttBroker()
{
	while(clients.size())
	{
		delete clients[0];
	}
}

// private constructor used by broker only
MqttClient::MqttClient(MqttBroker* parent, WiFiClient& new_client)
	: parent(parent)
{
	client = new WiFiClient(new_client);
	alive = millis()+5000;	// client expires after 5s if no CONNECT msg
}

MqttClient::MqttClient(MqttBroker* parent)
	: parent(parent)
{
		client = nullptr;

		if (parent) parent->addClient(this);
}

MqttClient::~MqttClient()
{
	close();
	delete client;
	Serial << "Client deleted" << endl;
}

void MqttClient::close(bool bSendDisconnect)
{
	debug("close " << id().c_str());
	mqtt_connected = false;
	if (client)
	{
		if (bSendDisconnect and client->connected())
		{
			message.create(MqttMessage::Type::Disconnect);
			message.sendTo(this);
		}
		client->stop();
	}

	if (parent)
	{
		parent->removeClient(this);
		parent=nullptr;
	}
}

void MqttClient::connect(std::string broker, uint16_t port, uint16_t ka)
{
	debug("cnx: closing");
	close();
	if (client) delete client;
	client = new WiFiClient;
	debug("Trying to connect to " << broker.c_str() << ':' << port);
	if (client->connect(broker.c_str(), port))
	{
	  debug("cnx: connecting");
		MqttMessage msg(MqttMessage::Type::Connect);
		msg.add("MQTT",4);
		msg.add(0x4);	// Mqtt protocol version 3.1.1
		msg.add(0x0);	// Connect flags         TODO user / name

		keep_alive = ka;
		msg.add(0x00);			// keep_alive
		msg.add((char)keep_alive);
		msg.add(clientId);
	  debug("cnx: mqtt connecting");
		msg.sendTo(this);
		msg.reset();
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
    if (client->connected())
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

MqttError MqttBroker::publish(const MqttClient* source, const Topic& topic, MqttMessage& msg)
{
	MqttError retval = MqttOk;

	debug("publish ");
	int i=0;
	for(auto client: clients)
	{
		i++;
#if TINY_MQTT_DEBUG
		Serial << "brk_" << (broker && broker->connected() ? "con" : "dis") <<
			 "	srce=" << (source->isLocal() ? "loc" : "rem") << " clt#" << i << ", local=" << client->isLocal() << ", con=" << client->connected() << endl;
#endif
		bool doit = false;
		if (broker && broker->connected())	// Broker is connected
		{
			//    ext broker -> clients or
			// or clients -> ext broker
			if (source == broker)	// broker -> clients
				doit = true;
			else									// clients -> broker
			{
				MqttError ret = broker->publish(topic, msg);
				if (ret != MqttOk) retval = ret;
			}
		}
		else // Disconnected: R7
		{
			// All is allowed
			doit = true;
		}
#if TINY_MQTT_DEBUG
		Serial << ", doit=" << doit << ' ';
#endif

		if (doit) retval = client->publish(topic, msg);
		debug("");
	}
	return retval;
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
			debug("closed");
		}
		else if (client && client->connected())
		{
			debug("pingreq");
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

void MqttClient::resubscribe()
{
	// TODO resubscription limited to 256 bytes
	if (subscriptions.size())
	{
		MqttMessage msg(MqttMessage::Type::Subscribe, 2);

		// TODO manage packet identifier
		msg.add(0);
		msg.add(0);

		for(auto topic: subscriptions)
		{
			msg.add(topic);
			msg.add(0);		// TODO qos
		}
		msg.sendTo(this);	// TODO return value
	}
}

MqttError MqttClient::subscribe(Topic topic, uint8_t qos)
{
	debug("subsribe(" << topic.c_str() << ")");
	MqttError ret = MqttOk;

 	subscriptions.insert(topic);

	if (parent==nullptr) // remote broker ?
	{
		debug("remote subscribe");
		MqttMessage msg(MqttMessage::Type::Subscribe, 2);

		// TODO manage packet identifier
		msg.add(0);
		msg.add(0);
		
		msg.add(topic);
		msg.add(qos);
		ret = msg.sendTo(this);
		
		// TODO we should wait (state machine) for SUBACK
	}
	return ret;
}

long MqttClient::counter=0;

void MqttClient::processMessage()
{
	counter++;
#if TINY_MQTT_DEBUG
if (message.type() != MqttMessage::Type::PingReq && message.type() != MqttMessage::Type::PingResp)
{
	Serial << "---> INCOMING " << _HEX(message.type()) << " client(" << (int)client << ':' << clientId << ") mem=" << ESP.getFreeHeap() << endl;
	// message.hexdump("Incoming");
}
#endif
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
			{
				MqttMessage msg(MqttMessage::Type::ConnAck);
				msg.add(0);	// Session present (not implemented)
				msg.add(0); // Connection accepted
				msg.sendTo(this);
			}
			break;

		case MqttMessage::Type::ConnAck:
			mqtt_connected = true;
			bclose = false;
			resubscribe();
			break;

		case MqttMessage::Type::SubAck:
		case MqttMessage::Type::PubAck:
			if (!mqtt_connected) break;
			// Ignore acks
			bclose = false;
			break;

		case MqttMessage::Type::PingResp:
			// TODO: no PingResp is suspicious (server dead)
			bclose = false;
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
		case MqttMessage::Type::UnSubscribe:
			{
				if (!mqtt_connected) break;
				payload = header+2;
				
				debug("subscribe loop");
				while(payload < message.end())
				{
					message.getString(payload, len);	// Topic
					debug( "  topic (" << std::string(payload, len) << ')');
					outstring("Subscribes", payload, len);
					// subscribe(Topic(payload, len));
					Topic topic(payload, len);
					if ((message.type() & 0XF0) == MqttMessage::Type::Subscribe)
						subscriptions.insert(topic);
					else
					{
						auto it=subscriptions.find(topic);
						if (it != subscriptions.end())
							subscriptions.erase(it);
					}
					payload += len;
					uint8_t qos = *payload++;
					debug("  qos=" << qos);
				}
				debug("end loop");
				bclose = false;
				// TODO SUBACK
			}
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
				if (parent)
				{
				  debug("publishing to parent");
					parent->publish(this, published, message);
				}
				else if (callback && subscriptions.find(published)!=subscriptions.end())
				{
					callback(this, published, nullptr, 0);	// TODO send the real payload
				}
				// TODO should send PUBACK
				bclose = false;
			}
			break;

		case MqttMessage::Type::Disconnect:
			// TODO should discard any will message
			if (!mqtt_connected) break;
			mqtt_connected = false;
			close(false);
			bclose=false;
			break;

		default:
			bclose=true;
			break;
	};
	if (bclose)
	{
		Serial << "*************** Error msg 0x" << _HEX(message.type());
		message.hexdump("-------ERROR ------");
		Serial << endl;
		close();
  }
	else
	{
		clientAlive(parent ? 5 : 0);
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
MqttError MqttClient::publish(const Topic& topic, const char* payload, size_t pay_length)
{
	MqttMessage msg(MqttMessage::Publish);
	msg.add(topic);
	msg.add(payload, pay_length, false);
	if (parent)
		return parent->publish(this, topic, msg);
	else if (client)
		return msg.sendTo(this);
	else
		return MqttNowhereToSend;
}

// republish a received publish if it matches any in subscriptions
MqttError MqttClient::publish(const Topic& topic, MqttMessage& msg)
{
	MqttError retval=MqttOk;

	debug("mqttclient publish " << subscriptions.size());
	for(const auto& subscription: subscriptions)
	{
		if (subscription.matches(topic))
		{
			debug(" match client=" << (int32_t)client << ", topic " << topic.str().c_str() << ' ');
			if (client)
			{
				retval = msg.sendTo(this);
			}
			else if (callback)
			{
				callback(this, topic, nullptr, 0);	// TODO Payload
			}
		}
	}
	return retval;
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
				else if (size > 500)	// TODO magic
				{
					state = Error;
				}
				else
				{
					buffer.reserve(size);
					state = VariableHeader;
				}
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
			hexdump("spurious");
			reset();
			break;
	}
	if (buffer.length() > MaxBufferLength)	// TODO magic 256 ?
	{
		debug("Too long " << state);
		reset();
	}
}

void MqttMessage::add(const char* p, size_t len, bool addLength)
{
	if (addLength)
	{
		incoming(len>>8);
		incoming(len & 0xFF);
	}
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

MqttError MqttMessage::sendTo(MqttClient* client)
{
	if (buffer.size()>2)
	{
		debug("sending " << buffer.size() << " bytes");
		encodeLength(&buffer[1], buffer.size()-2);
		// hexdump("snd");
		client->write(&buffer[0], buffer.size());
	}
	else
	{
		debug("??? Invalid send");
		return MqttInvalidMessage;
	}
	return MqttOk;
}

void MqttMessage::hexdump(const char* prefix) const
{
	uint16_t addr=0;
	const int bytes_per_row = 8;
	const char* hex_to_str = " | ";
	const char* separator = hex_to_str;
	const char* half_sep = " - ";
	std::string ascii;

	Serial << prefix << " size(" << buffer.size() << "), state=" << state << endl;

	for(const char chr: buffer)
	{
		if ((addr % bytes_per_row) == 0)
		{
			if (ascii.length()) Serial << hex_to_str << ascii << separator << endl;
			if (prefix) Serial << prefix << separator;
			ascii.clear();
		}
		addr++;
		if (chr<16) Serial << '0';
		Serial << _HEX(chr) << ' ';

		ascii += (chr<32 ? '.' : chr);
		if (ascii.length() == (bytes_per_row/2)) ascii += half_sep;
	}
	if (ascii.length())
	{
		while(ascii.length() < bytes_per_row+strlen(half_sep))
		{
			Serial << "   ";	// spaces per hexa byte
			ascii += ' ';
		}
		Serial << hex_to_str << ascii << separator;
	}

	Serial << endl;
}
