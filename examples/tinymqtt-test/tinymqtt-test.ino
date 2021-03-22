#define TINY_MQTT_DEBUG
#include <TinyMqtt.h>   // https://github.com/hsaturn/TinyMqtt
#include <Streaming.h>  // https://github.com/janelia-arduino/Streaming
#include <map>

/** 
  * Console allowing to make any kind of test.
  *
  * pros - Reduces internal latency (when publish is received by the same ESP)
  *      - Reduces wifi traffic
  *      - No need to have an external broker
	*      - can still report to a 'main' broker (TODO see documentation that have to be written)
	*      - accepts external clients
  *
  * cons - Takes more memory
	*      - a bit hard to understand
	*
  * This sounds crazy: a mqtt mqtt that do not need a broker !
  *  The use case arise when one ESP wants to publish topics and subscribe to them at the same time.
  *  Without broker, the ESP won't react to its own topics.
  *  
  *  TinyMqtt mqtt allows this use case to work.
  */

#include <my_credentials.h>

std::string topic="sensor/temperature";

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{ Serial << "--> " << srce->id().c_str() << ": ======> received " << topic.c_str() << endl; }

std::map<std::string, MqttClient*> clients;
std::map<std::string, MqttBroker*> brokers;


void setup()
{
  Serial.begin(115200);
	delay(500);
	Serial << endl << endl << endl
	<< "Demo started. Type help for more..." << endl
	<< "Connecting to '" << ssid << "' ";

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
	{ Serial << '-'; delay(500); }

  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;

	MqttBroker* broker = new MqttBroker(1883);
	broker->begin();
	brokers["broker"] = broker;
}

int getint(std::string& str, const int if_empty=0, char sep=' ')
{
	std::string sword;
	while(str.length() && str[0]!=sep)
	{
		sword += str[0]; str.erase(0,1);
	}
	while(str[0]==sep) str.erase(0,1);
	if (if_empty and sword.length()==0) sword=if_empty;
	return atoi(sword.c_str());
}

std::string getword(std::string& str, const char* if_empty=nullptr, char sep=' ')
{
	std::string sword;
	while(str.length() && str[0]!=sep)
	{
		sword += str[0]; str.erase(0,1);
	}
	while(str[0]==sep) str.erase(0,1);
	if (if_empty and sword.length()==0) return if_empty;
	return sword;
}

class automatic
{
	public:
		automatic(MqttClient* clt, uint32_t intervl)
		: client(clt), topic_(::topic)
		{
			interval(intervl);
		  autos[clt] = this;
		}

		void interval(uint32_t new_interval)
		{
			interval_ = new_interval;
			if (interval_<1000) interval_=1000;
			timer_ = millis() + interval_;
		}

		void loop_()
		{
			if (!bon) return;
			if (interval_ && millis() > timer_)
			{
				Serial << "AUTO PUBLISH " << interval_ << endl;
				timer_ += interval_;
				client->publish(topic_, std::string(String(15+millis()%10).c_str()));
			}
		}

		void topic(std::string new_topic) { topic_ = new_topic; }

		static void loop()
		{
			for(auto it: autos)
				it.second->loop_();
		}

		static void command(MqttClient* who, std::string cmd)
		{
			automatic* autop = nullptr;
			if (autos.find(who) != autos.end())
			{
				autop=autos[who];
			}
			std::string s = getword(cmd);
			if (compare(s, "create"))
			{
				std::string seconds=getword(cmd, "10000");
				if (autop) delete autop;
				std::string top = getword(cmd, ::topic.c_str());
				autos[who] = new automatic(who, atol(seconds.c_str()));
				autos[who]->topic(top);
				autos[who]->bon=true;
				Serial << "New auto (" << seconds.c_str() << " topic:" << top.c_str() << ')' << endl;
			}
			else if (autop)
			{
				while(s.length())
				{
					if (s=="on")
					{
						autop->bon = true;
						autop->interval(autop->interval_);
					}
					else if (s=="off")
						autop->bon=false;
					else if (s=="interval")
					{
						int32_t i=getint(cmd);
						if (i)
							autop->interval(atol(s.c_str()));
						else
							Serial << "Bad value" << endl;
					}
				  else if (s=="view")
					{
						Serial << "  automatic "
							<< (int32_t)autop->client
							<< " interval " << autop->interval_
							<< (autop->bon ? " on" : " off") << endl;
					}
					else
					{
						Serial << "Unknown auto command (" << s.c_str() << ")" << endl;
						break;
					}
					s=getword(cmd);
				}
			}
			else if (who==nullptr)
			{
				for(auto it: autos)
					command(it.first, s+' '+cmd);
			}
			else
				Serial << "what ? (" << s.c_str() << ")" << endl;
		}

		static void help()
		{
					Serial << "    auto [$id] on/off" << endl;
					Serial << "    auto [$id] view" << endl;
					Serial << "    auto [$id] create [millis] [topic]" << endl;
		}

	private:
		MqttClient* client;
		uint32_t interval_;
		uint32_t timer_;
		std::string topic_;
		bool bon=false;
		static std::map<MqttClient*, automatic*> autos;
};
std::map<MqttClient*, automatic*> automatic::autos;

bool compare(std::string s, const char* cmd)
{
	if (s.length()==0 or s.length()>strlen(cmd))  return false;
	return strncmp(cmd, s.c_str(), s.length())==0;
}

using ClientFunction = void(*)(std::string& cmd, MqttClient* publish);

void loop()
{
	for(auto it: brokers)
		it.second->loop();

	for(auto it: clients)
		it.second->loop();

	automatic::loop();

	if (Serial.available())
	{
		static std::string cmd;
		char c=Serial.read();

		if (c==10 or c==14)
		{
			Serial << "----------------[ " << cmd.c_str() << " ]--------------" << endl;
			static std::string last_cmd;
			if (cmd=="!")
				cmd=last_cmd;
			else
				last_cmd=cmd;
			while(cmd.length())
			{
				std::string s;
				MqttBroker* broker = nullptr;
				MqttClient* client = nullptr;

				// client.function notation
				// ("a.fun " becomes "fun a ")
				if (cmd.find('.') != std::string::npos)
				{
					s=getword(cmd, nullptr, '.');

					if (clients.find(s) != clients.end())
					{
						client = clients[s];
					}
					else if (brokers.find(s) != brokers.end())
					{
						broker = brokers[s];
					}
					else
					{
						Serial << "Unknown class (" << s.c_str() << ")" << endl;
						cmd="";
					}
				}
				
				s = getword(cmd);
				if (compare(s, "delete"))
				{
					if (client==nullptr && broker==nullptr)
					{
						s = getword(cmd);
						if (clients.find(s) != clients.end())
						{
							client = clients[s];
						}
						else if (brokers.find(s) != brokers.end())
						{
							broker = brokers[s];
						}
						else
							Serial << "Unable to find (" << s.c_str() << ")" << endl;
					}
					if (client)
					{
						clients.erase(s);
						for (auto it: clients)
						{
							if (it.second != client) continue;
							Serial << "deleted" << endl;
							clients.erase(it.first);
							break;
						}
						cmd += " ls";
					}
					else if (broker)
					{
						for(auto it: brokers)
						{
							Serial << (int32_t)it.second << '/' << (int32_t)broker << endl;
							if (broker != it.second) continue;
							Serial << "deleted" << endl;
							brokers.erase(it.first);
							break;
						}
						cmd += " ls";
					}
					else
						Serial << "Nothing to delete" << endl;
				}
				else if (broker)
				{
					if (compare(s,"connect"))
				  {
						Serial << "NYI" << endl;
				  }
					else if (compare(s, "view"))
					{
						broker->dump();
					}
			  }
				else if (client)
				{
					if (compare(s,"connect"))
					{
						client->connect(getword(cmd,"192.168.1.40").c_str(), 1883);
						Serial << (client->connected() ? "connected." : "not connected") << endl;
					}
					else if (compare(s,"publish"))
					{
						auto ok=client->publish(getword(cmd, topic.c_str()));
						if (ok != MqttOk)
						{
							Serial << "## ERROR " << ok << endl;
						}
					}
					else if (compare(s,"subscribe"))
					{
						client->subscribe(getword(cmd, topic.c_str()));
					}
					else if (compare(s, "view"))
					{
						client->dump();
					}
				}
				else if (compare(s, "auto"))
				{
					automatic::command(client, cmd);
					if (client == nullptr)
						cmd.clear();
				}
				else if (compare(s, "broker"))
				{
					std::string id=getword(cmd);
					if (id.length() or brokers.find(id)!=brokers.end())
					{
						int port=getint(cmd, 0);
						if (port)
						{
							MqttBroker* broker = new MqttBroker(port);
							broker->begin();

							brokers[id] = broker;
							Serial << "new broker (" << id.c_str() << ")" << endl;
						}
						else
							Serial << "Missing port" << endl;
					}
					else
						Serial << "Missing or existing broker name (" << id.c_str() << ")" << endl;
					cmd+=" ls";
				}
				else if (compare(s, "client"))
				{
					std::string id=getword(cmd);
					if (id.length() or clients.find(id)!=clients.end())
					{
						s=getword(cmd);	// broker name
						if (s=="" or brokers.find(s) != brokers.end())
						{
							MqttBroker* broker = nullptr;
							if (s.length()) broker = brokers[s];
							MqttClient* client = new MqttClient(broker);
							client->id(id);
							clients[id]=client;
							client->setCallback(onPublish);
							client->subscribe(topic);
							Serial << "new client (" << id.c_str() << ", " << s.c_str() << ')' << endl;
						}
						else if (s.length())
						{
							Serial << " not found." << endl;
						}
					}
					else
						Serial << "Missing or existing client name" << endl;
					cmd+=" ls";
				}
				else if (compare(s, "ls") or compare(s, "view"))
				{
					Serial << "--< " << clients.size() << " client/s. >--" << endl;
					for(auto it: clients)
					{
						Serial << "  "; it.second->dump();
					}

					Serial << "--< " << brokers.size() << " brokers/s. >--" << endl;
					for(auto it: brokers)
					{
						Serial << " ==[ Broker: " << it.first.c_str() << " ]== ";
						it.second->dump();
					}
				}
				else if (compare(s, "reset"))
					ESP.restart();
				else if (compare(s, "ip"))
					Serial << "IP: " << WiFi.localIP() << endl;
				else if (compare(s,"help"))
				{
					Serial << "syntax:" << endl;
					Serial << "  MqttBroker:" << endl;
					Serial << "    broker {name} {port} : create a new broker" << endl;
					Serial << endl;
					Serial << "  MqttClient:" << endl;
					Serial << "    client {name} {parent broker} : create a client then" << endl;
					Serial << "      name.connect  [ip]" << endl;
					Serial << "      name.subscribe [topic]" << endl;
					Serial << "      name.publish [topic]" << endl;
					Serial << "      name.view" << endl;
					Serial << "      name.delete" << endl;

					automatic::help();
					Serial << endl;
					Serial << "    help" << endl;
					Serial << "    ls / ip / reset" << endl;
					Serial << "    !  repeat last command" << endl;
					Serial << endl;
					Serial << "  $id : name of the client." << endl;
					Serial << "  default topic is '" << topic.c_str() << "'" << endl;
					Serial << endl;
				}
				else
				{
					while(s[0]==' ') s.erase(0,1);
					if (s.length())
						Serial << "Unknown command (" << s.c_str() << ")" << endl;
				}
			}
		}
		else
		{
			cmd=cmd+c;
		}
	}
}
