#include "TinyMqtt.h"
#include <sstream>

MqttBroker::MqttBroker(uint16_t port)
{
	server = new TcpServer(port);
#ifdef TCP_ASYNC
	server->onClient(onClient, this);
#endif
}

MqttBroker::~MqttBroker()
{
	while(clients.size())
	{
		delete clients[0];
	}
	delete server;
}

// private constructor used by broker only
MqttClient::MqttClient(MqttBroker* parent, TcpClient* new_client)
	: parent(parent)
{
#ifdef TCP_ASYNC
	client = new_client;
	client->onData(onData, this);
	// client->onConnect() TODO
	// client->onDisconnect() TODO
#else
	client = new WiFiClient(*new_client);
#endif
	alive = millis()+5000;	// TODO MAGIC client expires after 5s if no CONNECT msg
}

MqttClient::MqttClient(MqttBroker* parent, const std::string& id)
	: parent(parent), clientId(id)
{
		client = nullptr;

		if (parent) parent->addClient(this);
}

MqttClient::~MqttClient()
{
	close();
	delete client;
}

void MqttClient::close(bool bSendDisconnect)
{
	debug("close " << id().c_str());
	mqtt_connected = false;
	if (client)	// connected to a remote broker
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
		parent = nullptr;
	}
}

void MqttClient::connect(MqttBroker* parentBroker)
{
	close();
	parent = parentBroker;
}

void MqttClient::connect(std::string broker, uint16_t port, uint16_t ka)
{
	debug("MqttClient::connect");
	keep_alive = ka;
	close();
	if (client) delete client;
	client = new TcpClient;

	debug("Trying to connect to " << broker.c_str() << ':' << port);
#ifdef TCP_ASYNC
	client->onData(onData, this);
	client->onConnect(onConnect, this);
	client->connect(broker.c_str(), port);
#else
	if (client->connect(broker.c_str(), port))
	{
		onConnect(this, client);
	}
#endif
}

void MqttBroker::addClient(MqttClient* client)
{
		clients.push_back(client);
}

void MqttBroker::connect(const std::string& host, uint16_t port)
{
	if (broker == nullptr) broker = new MqttClient;
	broker->connect(host, port);
	broker->parent = this;	// Because connect removed the link
}

void MqttBroker::removeClient(MqttClient* remove)
{
  for(auto it=clients.begin(); it!=clients.end(); it++)
	{
		auto client=*it;
		if (client==remove)
		{
			// TODO if this broker is connected to an external broker
			// we have to unsubscribe remove's topics.
			// (but doing this, check that other clients are not subscribed...)
			// Unless -> we could receive useless messages
			//        -> we are using (memory) one IndexedString plus its string for nothing.
			debug("Remove " << clients.size());
			clients.erase(it);
			debug("Client removed " << clients.size());
			return;
		}
	}
	debug("Error cannot remove client");	// TODO should not occur
}

void MqttBroker::onClient(void* broker_ptr, TcpClient* client)
{
	MqttBroker* broker = static_cast<MqttBroker*>(broker_ptr);

	broker->addClient(new MqttClient(broker, client));
	debug("New client");
}

void MqttBroker::loop()
{
#ifndef TCP_ASYNC
  WiFiClient client = server->available();

  if (client)
	{
		onClient(this, &client);
	}
#endif
	if (broker)
	{
		// TODO should monitor broker's activity.
		// 1 When broker disconnect and reconnect we have to re-subscribe
		broker->loop();
	}


  // for(auto it=clients.begin(); it!=clients.end(); it++)
	// use index because size can change during the loop
	for(size_t i=0; i<clients.size(); i++)
	{
		auto client = clients[i];
    if (client->connected())
    {
			client->loop();
		}
		else
		{
      debug("Client " << client->id().c_str() << "  Disconnected, parent=" << (dbg_ptr)client->parent);
			// Note: deleting a client not added by the broker itself will probably crash later.
			delete client;
			break;
    }
	}
}

MqttError MqttBroker::subscribe(const Topic& topic, uint8_t qos)
{
	if (broker && broker->connected())
	{
		return broker->subscribe(topic, qos);
	}
	return MqttNowhereToSend;
}

MqttError MqttBroker::publish(const MqttClient* source, const Topic& topic, MqttMessage& msg) const
{
	MqttError retval = MqttOk;

	debug("publish ");
	int i=0;
	for(auto client: clients)
	{
		i++;
#ifdef TINY_MQTT_DEBUG
		Serial << "brk_" << (broker && broker->connected() ? "con" : "dis") <<
			 "	srce=" << (source->isLocal() ? "loc" : "rem") << " clt#" << i << ", local=" << client->isLocal() << ", con=" << client->connected() << endl;
#endif
		bool doit = false;
		if (broker && broker->connected())	// this (MqttBroker) is connected (to a external broker)
		{
			// ext_broker -> clients or clients -> ext_broker
			if (source == broker)	// external broker -> internal clients
				doit = true;
			else									// external clients -> this broker
			{
				// As this broker is connected to another broker, simply forward the msg
				MqttError ret = broker->publishIfSubscribed(topic, msg);
				if (ret != MqttOk) retval = ret;
			}
		}
		else // Disconnected
		{
			doit = true;
		}
#ifdef TINY_MQTT_DEBUG
		Serial << ", doit=" << doit << ' ';
#endif

		if (doit) retval = client->publishIfSubscribed(topic, msg);
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
			client->write((const char*)(&pingreq), 2);
			clientAlive(0);

			// TODO when many MqttClient passes through a local broker
			// there is no need to send one PingReq per instance.
		}
	}
#ifndef TCP_ASYNC
	while(client && client->available()>0)
	{
		message.incoming(client->read());
		if (message.type())
		{
			processMessage(&message);
			message.reset();
		}
	}
#endif
}

void MqttClient::onConnect(void *mqttclient_ptr, TcpClient*)
{
	MqttClient* mqtt = static_cast<MqttClient*>(mqttclient_ptr);
	debug("cnx: connecting");
	MqttMessage msg(MqttMessage::Type::Connect);
	msg.add("MQTT",4);
	msg.add(0x4);	// Mqtt protocol version 3.1.1
	msg.add(0x0);	// Connect flags         TODO user / name

	msg.add(0x00);			// keep_alive
	msg.add((char)mqtt->keep_alive);
	msg.add(mqtt->clientId);
	debug("cnx: mqtt connecting");
	msg.sendTo(mqtt);
	msg.reset();
	debug("cnx: mqtt sent " << (dbg_ptr)mqtt->parent);

	mqtt->clientAlive(0);
}

#ifdef TCP_ASYNC
void MqttClient::onData(void* client_ptr, TcpClient*, void* data, size_t len)
{
	char* char_ptr = static_cast<char*>(data);
	MqttClient* client=static_cast<MqttClient*>(client_ptr);
	while(len>0)
	{
		client->message.incoming(*char_ptr++);
		if (client->message.type())
		{
			client->processMessage(&client->message);
			client->message.reset();
		}
		len--;
	}
}
#endif

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

	if (parent==nullptr) // remote broker
	{
		return sendTopic(topic, MqttMessage::Type::Subscribe, qos);
	}
	else
	{
		return parent->subscribe(topic, qos);
	}
	return ret;
}

MqttError MqttClient::unsubscribe(Topic topic)
{
	auto it=subscriptions.find(topic);
	if (it != subscriptions.end())
	{
		subscriptions.erase(it);
		if (parent==nullptr) // remote broker
		{
			return sendTopic(topic, MqttMessage::Type::UnSubscribe, 0);
		}
	}
	return MqttOk;
}

MqttError MqttClient::sendTopic(const Topic& topic, MqttMessage::Type type, uint8_t qos)
{
	MqttMessage msg(type, 2);

	// TODO manage packet identifier
	msg.add(0);
	msg.add(0);

	msg.add(topic);
	msg.add(qos);

	// TODO instead we should wait (state machine) for SUBACK / UNSUBACK ?
	return msg.sendTo(this);
}

long MqttClient::counter=0;

void MqttClient::processMessage(MqttMessage* mesg)
{
	counter++;
#ifdef TINY_MQTT_DEBUG
if (mesg->type() != MqttMessage::Type::PingReq && mesg->type() != MqttMessage::Type::PingResp)
{
	#ifdef NOT_ESP_CORE
		Serial << "---> INCOMING " << _HEX(mesg->type()) << " client(" << (dbg_ptr)client << ':' << clientId << ") mem=" << " ESP.getFreeHeap() "<< endl;
	#else
		Serial << "---> INCOMING " << _HEX(mesg->type()) << " client(" << (dbg_ptr)client << ':' << clientId << ") mem=" << ESP.getFreeHeap() << endl;
	#endif
	// mesg->hexdump("Incoming");
	mesg->hexdump("Incoming");
}
#endif
  auto header = mesg->getVHeader();
  const char* payload;
  uint16_t len;
	bool bclose=true;

	switch(mesg->type() & 0XF0)
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
			mesg->getString(payload, len);
			clientId = std::string(payload, len);
			payload += len;

			if (mqtt_flags & FlagWill)	// Will topic
			{
				mesg->getString(payload, len);	// Will Topic
				payload += len;

				mesg->getString(payload, len);	// Will Message
				payload += len;
			}
			// FIXME forgetting credential is allowed (security hole)
			if (mqtt_flags & FlagUserName)
			{
				mesg->getString(payload, len);
				if (!parent->checkUser(payload, len)) break;
				payload += len;
			}
			if (mqtt_flags & FlagPassword)
			{
				mesg->getString(payload, len);
				if (!parent->checkPassword(payload, len)) break;
				payload += len;
			}

      #ifdef TINY_MQTT_DEBUG
			  Serial << "Connected client:" << clientId.c_str() << ", keep alive=" << keep_alive << '.' << endl;
      #endif
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
				client->write((const char*)(&pingreq), 2);
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
				
				debug("un/subscribe loop");
				while(payload < mesg->end())
				{
					mesg->getString(payload, len);	// Topic
					debug( "  topic (" << std::string(payload, len) << ')');
					// subscribe(Topic(payload, len));
					Topic topic(payload, len);
					payload += len;
					if ((mesg->type() & 0XF0) == MqttMessage::Type::Subscribe)
					{
						uint8_t qos = *payload++;
						if (qos != 0) debug("Unsupported QOS" << qos << endl);
						subscriptions.insert(topic);
					}
					else
					{
						auto it=subscriptions.find(topic);
						if (it != subscriptions.end())
							subscriptions.erase(it);
					}
				}
				debug("end loop");
				bclose = false;
				// TODO SUBACK
			}
			break;

		case MqttMessage::Type::UnSuback:
			if (!mqtt_connected) break;
			bclose = false;
			break;

		case MqttMessage::Type::Publish:
			#ifdef TINY_MQTT_DEBUG
				Serial << "publish " << mqtt_connected << '/' << (long) client << endl;
			#endif
			if (mqtt_connected or client == nullptr)
			{
				uint8_t qos = mesg->type() & 0x6;
				payload = header;
				mesg->getString(payload, len);
				Topic published(payload, len);
				payload += len;
				// Serial << "Received Publish (" << published.str().c_str() << ") size=" << (int)len
				// << '(' << std::string(payload, len).c_str() << ')'	<< " msglen=" << mesg->length() << endl;
				if (qos) payload+=2;	// ignore packet identifier if any
				len=mesg->end()-payload;
				// TODO reset DUP
				// TODO reset RETAIN

				if (parent==nullptr or client==nullptr)	// internal MqttClient receives publish
				{
					#ifdef TINY_MQTT_DEBUG
						Serial << (isSubscribedTo(published) ? "not" : "") << " subscribed.\n";
						Serial << "has " << (callback ? "" : "no ") << " callback.\n";
					#endif
					if (callback and isSubscribedTo(published))
					{
						callback(this, published, payload, len);	// TODO send the real payload
					}
				}
				else if (parent) // from outside to inside
				{
				  debug("publishing to parent");
					parent->publish(this, published, *mesg);
				}
				bclose = false;
			}
			break;

		case MqttMessage::Type::Disconnect:
			// TODO should discard any will msg
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
    #ifdef TINY_MQTT_DEBUG
		  Serial << "*************** Error msg 0x" << _HEX(mesg->type());
		  mesg->hexdump("-------ERROR ------");
		  dump();
		  Serial << endl;
    #endif
		close();
  }
	else
	{
		clientAlive(parent ? 5 : 0);
	}
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
	msg.complete();

	if (parent)
	{
		return parent->publish(this, topic, msg);
	}
	else if (client)
		return msg.sendTo(this);
	else
		return MqttNowhereToSend;
}

// republish a received publish if it matches any in subscriptions
MqttError MqttClient::publishIfSubscribed(const Topic& topic, MqttMessage& msg)
{
	MqttError retval=MqttOk;

	debug("mqttclient publish " << subscriptions.size());
	if (isSubscribedTo(topic))
	{
		if (client)
			retval = msg.sendTo(this);
		else
		{
			processMessage(&msg);

			#ifdef TINY_MQTT_DEBUG
				Serial << "Should call the callback ?\n";
			#endif
			// callback(this, topic, nullptr, 0);	// TODO Payload
		}
	}
	return retval;
}

bool MqttClient::isSubscribedTo(const Topic& topic) const
{
	for(const auto& subscription: subscriptions)
		if (subscription.matches(topic))
			return true;

	return false;
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
			size=MaxBufferLength;
			state = Length;
			break;
		case Length:

		  if (size==MaxBufferLength)
        size = in_byte & 0x7F;
		  else
		    size += static_cast<uint16_t>(in_byte & 0x7F)<<7;

			if (size > MaxBufferLength)
				state = Error;
			else if ((in_byte & 0x80) == 0)
			{
				vheader = buffer.length();
				if (size==0)
					state = Complete;
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
			#ifdef TINY_MQTT_DEBUG
			  Serial << "Spurious " << _HEX(in_byte) << endl;
			  hexdump("spurious");
      #endif
			reset();
			break;
	}
	if (buffer.length() > MaxBufferLength)
	{
		debug("Too long " << state);
		reset();
	}
}

void MqttMessage::add(const char* p, size_t len, bool addLength)
{
	if (addLength)
	{
		buffer.reserve(buffer.length()+2);
		incoming(len>>8);
		incoming(len & 0xFF);
	}
	while(len--) incoming(*p++);
}

void MqttMessage::encodeLength()
{
	if (state != Complete)
	{
		int length = buffer.size()-3;	// 3 = 1 byte for header + 2 bytes for pre-reserved length field.
    buffer[1] = 0x80 | (length & 0x7F);
    buffer[2] = (length >> 7);
    vheader = 3;
      
    // We could check that buffer[2] < 128 (end of length encoding)
    state = Complete;
  }
};

MqttError MqttMessage::sendTo(MqttClient* client)
{
	if (buffer.size())
	{
		debug("sending " << buffer.size() << " bytes");
		encodeLength();
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
  (void)prefix;
#ifdef TINY_MQTT_DEBUG
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
#endif
}
