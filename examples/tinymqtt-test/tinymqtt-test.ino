#include <TinyMqtt.h>   // https://github.com/hsaturn/TinyMqtt
#include <Streaming.h>  // https://github.com/janelia-arduino/Streaming
#include <map>

/** 
  * Local broker that accept connections
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

MqttBroker broker(1883);

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{ Serial << "--> " << srce->id().c_str() << ": ======> received " << topic.c_str() << endl; }

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

	broker.begin();
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
						int32_t i=atol(getword(cmd).c_str());
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

std::map<std::string, MqttClient*> clients;

using ClientFunction = void(*)(std::string& cmd, MqttClient* publish);

void clientCommand(std::string& cmd, ClientFunction func, bool canBeNull=false)
{
	std::string s=getword(cmd);
	bool found = clients.find(s) != clients.end();

	if (canBeNull && found==false)
	{
		cmd += ' ' + s;
	}

	if (found or canBeNull)
	{
		MqttClient* publish = publish = clients[s];
		func(cmd, publish);
	}
	else
	{
		Serial << "client not found (" << s.c_str() << ")" << endl;
		cmd="";
	}
}

void loop()
{
	broker.loop();

	for(auto it: clients)
		it.second->loop();

	automatic::loop();

	if (Serial.available())
	{
		static std::string cmd;
		char c=Serial.read();

		if (c==10 or c==14)
		{
			Serial << "------------------------------------------------------" << endl;
			static std::string last_cmd;
			if (cmd=="!")
				cmd=last_cmd;
			else
				last_cmd=cmd;
			while(cmd.length())
			{
				std::string s;

				// client.function notation
				// ("a.fun " becomes "fun a ")
				if (cmd.find('.') != std::string::npos)
				{
					std::string copy(cmd);
					s=getword(copy, nullptr, '.');

					if (clients.find(s) != clients.end())
					{
						std::string s2 = getword(copy);
						cmd=s2+' '+s+' '+copy;
					}
					else
					{
						Serial << "Unknown client (" << s.c_str() << ")" << endl;
						cmd="";
					}
				}
				
				s = getword(cmd);
				if (compare(s,"connect"))
				{
					clientCommand(cmd, [](std::string& cmd, MqttClient* publish)
							{ publish->connect(getword(cmd,"192.168.1.40").c_str(), 1883);
							  Serial << (publish->connected() ? "connected." : "not connected") << endl;
							});
				}
				else if (compare(s,"publish"))
				{
					clientCommand(cmd, [](std::string& cmd, MqttClient* publish)
							{ publish->publish(getword(cmd, topic.c_str())); });
				}
				else if (compare(s,"subscribe"))
				{
					clientCommand(cmd, [](std::string& cmd, MqttClient* publish)
							{ publish->subscribe(getword(cmd, topic.c_str())); });
				}
				else if (compare(s, "view"))
				{
					clientCommand(cmd, [](std::string& cmd, MqttClient* publish)
							{ publish->dump(); });
				}
				else if (compare(s, "auto"))
				{
					clientCommand(cmd, [](std::string& cmd, MqttClient* publish)
							{ automatic::command(publish, cmd);
							if (publish == nullptr)
							cmd.clear();
							}, true);
				}
				else if (compare(s, "new"))
				{
					std::string id=getword(cmd);
					if (id.length())
					{
						MqttClient* client = new MqttClient(&broker);
						client->id(id);
						clients[id]=client;
						client->setCallback(onPublish);
						client->subscribe(topic);
					}
					else
						Serial << "missing id" << endl;
					cmd+=" ls";
				}
				else if (compare(s, "delete"))
				{
					s = getword(cmd);
					auto it=clients.find(s);
					if (it != clients.end())
					{
						delete it->second;
						clients.erase(it);
						cmd+=" ls";
					}
					else
						Serial << "Unknown client (" << s.c_str() << ")" << endl;
				}
				else if (compare(s, "ls"))
				{
					Serial << "main  : " << clients.size() << " client/s." << endl;
					for(auto it: clients)
					{
						Serial << "  "; it.second->dump();
					}
					broker.dump();
				}
				else if (compare(s, "reset"))
					ESP.restart();
				else if (compare(s, "ip"))
					Serial << "IP: " << WiFi.localIP() << endl;
				else if (compare(s,"help"))
				{
					Serial << "syntax:" << endl;
					Serial << "    new/delete $id" << endl;
					Serial << "    connect $id [ip]" << endl;
					Serial << "    subscribe $id [topic]" << endl;
					Serial << "    publish $id [topic]" << endl;
					Serial << "    view $id " << endl;
					automatic::help();
					Serial << endl;
					Serial << "    help" << endl;
					Serial << "    ls / ip / reset" << endl;
					Serial << "    !  repeat last command" << endl;
					Serial << endl;
					Serial << "  $id : name of the client." << endl;
					Serial << "  default topic is '" << topic.c_str() << "'" << endl;
					Serial << endl;
					Serial << "   'function client args' can be written 'client.function args'" << endl;
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
