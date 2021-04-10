#include <TinyMqtt.h>       // https://github.com/hsaturn/TinyMqtt
#include <MqttStreaming.h>
#include <ESP8266mDNS.h>

#include <sstream>
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
  */

#include <my_credentials.h>

std::string topic="sensor/temperature";

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
	Serial << "--> " << srce->id().c_str() << ": ======> received " << topic.c_str();
	if (payload) Serial << ", payload[" << length << "]=[";
	while(length--)
	{
		const char c=*payload++;
		if (c!=10 and c!=13 and c <32) Serial << '?';
		Serial << *payload++;
	}
	Serial<< endl;
}

std::map<std::string, MqttClient*> clients;
std::map<std::string, MqttBroker*> brokers;

void setup()
{
	WiFi.persistent(false); // https://github.com/esp8266/Arduino/issues/1054
  Serial.begin(115200);
	delay(500);
	Serial << endl << endl << endl
	<< "Connecting to '" << ssid << "' ";

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
	{ Serial << '-'; delay(500); }

  Serial << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;
	Serial << "Type help for more..." << endl;

  const char* name="tinytest";
  Serial << "Starting MDNS, name= " << name;
  if (!MDNS.begin(name))
    Serial << "  error, not available." << endl;
  else
    Serial << " ok." << endl;


	MqttBroker* broker = new MqttBroker(1883);
	broker->begin();
	brokers["broker"] = broker;
}

int getint(std::string& str, const int if_empty=0)
{
	std::string sword;
	while(str.length() && str[0]>='0' && str[0]<='9')
	{
		sword += str[0]; str.erase(0,1);
	}
	while(str[0]==' ') str.erase(0,1);
	if (if_empty and sword.length()==0) return if_empty;
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

bool isaddr(std::string s)
{
	if (s.length()==0 or s.length()>3) return false;
	for(char c: s)
		if (c<'0' or c>'9') return false;
	return true;
}

std::string getip(std::string& str, const char* if_empty=nullptr, char sep=' ')
{
	std::string addr=getword(str, if_empty, sep);
	std::string ip=addr;
	std::vector<std::string> build;
	while(ip.length())
	{
		std::string b=getword(ip,nullptr,'.');
		if (isaddr(b) && build.size()<4)
		{
			build.push_back(b);
		}
		else
			return addr;
	}
	IPAddress local=WiFi.localIP();
	addr="";
	while(build.size()!=4)
	{
		std::stringstream b;
		b << (int)local[3-build.size()];
		build.insert(build.begin(), b.str());
	}
	for(std::string s: build)
	{
		if (addr.length()) addr += '.';
		addr += s;
	}
	return addr;
}

std::map<std::string, std::string> vars;

std::set<std::string> commands = {
	"auto", "broker", "blink", "client", "connect",
	"create", "delete", "help", "interval",
	"ls", "ip", "off", "on", "set",
	"publish", "reset", "subscribe", "unsubscribe", "view"
};

void getCommand(std::string& search)
{
	while(search[0]==' ') search.erase(0,1);
	if (search.length()==0) return;
	std::string matches;
	int count=0;
	for(std::string cmd: commands)
	{
		if (cmd.substr(0, search.length()) == search)
		{
			if (count) matches +=", ";
			count++;
			matches += cmd;
		}
	}
	if (count==1)
		search = matches;
	else if (count>1)
	{
		Serial << "Ambiguous command: " << matches << endl;
		search="";
	}
}

void replace(const char* d, std::string& str, std::string srch, std::string to)
{
	if (d[0] && d[1])
	{
		srch=d[0]+srch+d[1];
		to=d[0]+to+d[1];

		size_t pos = 0;
		while((pos=str.find(srch, pos)) != std::string::npos)
		{
			str.erase(pos, srch.length());
			str.insert(pos, to);
			pos += to.length();
		}
	}
}

void replaceVars(std::string& cmd)
{
	cmd = ' '+cmd+' ';

	for(auto it: vars)
	{
		replace("..", cmd, it.first, it.second);
		replace(". ", cmd, it.first, it.second);
		replace(" .", cmd, it.first, it.second);
		replace("  ", cmd, it.first, it.second);
	}
	cmd.erase(0, cmd.find_first_not_of(" "));
	cmd.erase(cmd.find_last_not_of(" ")+1);
}

// publish at regular interval
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
					Serial << "    auto [$id] interval [s]" << endl;
					Serial << "    auto [$id] create [millis] [topic]" << endl;
		}

	private:
		MqttClient* client;
		uint32_t interval_;
		uint32_t timer_;
		std::string topic_;
		bool bon=false;
		static std::map<MqttClient*, automatic*> autos;
		float temp=19;
};
std::map<MqttClient*, automatic*> automatic::autos;

bool compare(std::string s, const char* cmd)
{
	if (s.length()==0 or s.length()>strlen(cmd))  return false;
	return strncmp(cmd, s.c_str(), s.length())==0;
}

using ClientFunction = void(*)(std::string& cmd, MqttClient* publish);

uint32_t blink_ms_on[16];
uint32_t blink_ms_off[16];
uint32_t blink_next[16];
bool blink_state[16];
int16_t blink;
void loop()
{
	auto ms=millis();
	int8_t out=0;
	int16_t blink_bits = blink;
	while(blink_bits)
	{
		if (blink_ms_on[out] and ms > blink_next[out])
		{
			if (blink_state[out])
			{
				blink_next[out] += blink_ms_on[out];
				digitalWrite(out, LOW);
			}
			else
			{
				blink_next[out] += blink_ms_off[out];
				digitalWrite(abs(out), HIGH);
			}
			blink_state[out] = not blink_state[out];
		}
		blink_bits >>=1;
		out++;
	}

	static long count;
  MDNS.update();

	if (MqttClient::counter != count)
	{
		Serial << "# " << MqttClient::counter << endl;
		count = MqttClient::counter;
	}
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

			if (cmd.substr(0,3)!="set") replaceVars(cmd);
			while(cmd.length())
			{
				MqttError retval = MqttOk;

				std::string s;
				MqttBroker* broker = nullptr;
				MqttClient* client = nullptr;

				// client.function notation
				// ("a.fun " becomes "fun a ")
				if (cmd.find('.') != std::string::npos &&
						cmd.find('.') < cmd.find(' '))
				{
					s=getword(cmd, nullptr, '.');

					if (s.length())
					{
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
				}
				
				s = getword(cmd);
				if (s.length()) getCommand(s);
				if (s.length()==0)
				{}
				else if (compare(s, "delete"))
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
						for (auto it: clients)
						{
							if (it.second != client) continue;
							Serial << "deleted" << endl;
							delete (it.second);
							clients.erase(it.first);
							break;
						}
						cmd += " ls";
					}
					else if (broker)
					{
						for(auto it: brokers)
						{
							if (broker != it.second) continue;
							Serial << "deleted" << endl;
							delete (it.second);
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
						client->connect(getip(cmd,"192.168.1.40").c_str(), getint(cmd, 1883), getint(cmd, 60));
						Serial << (client->connected() ? "connected." : "not connected") << endl;
					}
					else if (compare(s,"publish"))
					{
						while (cmd[0]==' ') cmd.erase(0,1);
						retval = client->publish(getword(cmd, topic.c_str()), cmd.c_str(), cmd.length());
						cmd="";	// remove payload
					}
					else if (compare(s,"subscribe"))
					{
						client->subscribe(getword(cmd, topic.c_str()));
					}
					else if (compare(s, "unsubscribe"))
					{
						client->unsubscribe(getword(cmd, topic.c_str()));
					}
					else if (compare(s, "view"))
					{
						client->dump();
					}
				}
				else if (compare(s, "blink"))
				{
					int8_t blink_nr = getint(cmd, -1);
					if (blink_nr >= 0)
					{
						blink_ms_on[blink_nr]=getint(cmd, blink_ms_on[blink_nr]);
						blink_ms_off[blink_nr]=getint(cmd, blink_ms_on[blink_nr]);
						pinMode(blink_nr, OUTPUT);
						blink_next[blink_nr] = millis();
						Serial << "Blink " << blink_nr << ' ' << (blink_ms_on[blink_nr] ? "on" : "off") << endl;
						if (blink_ms_on[blink_nr])
							blink |= 1<< blink_nr;
						else
						{
							blink &= ~(1<< blink_nr);
						}
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
				else if (compare(s, "set"))
				{
					std::string name(getword(cmd));
					if (name.length()==0)
					{
						for(auto it: vars)
						{
							Serial << "  " << it.first << " -> " << it.second << endl;
						}
					}
					else if (commands.find(name) != commands.end())
					{
						Serial << "Reserved keyword (" << name << ")" << endl;
						cmd.clear();
					}
					else
					{
						if (cmd.length())
						{
							vars[name] = cmd;
							cmd.clear();
						}
						else if (vars.find(name) != vars.end())
							vars.erase(vars.find(name));
					}
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
					Serial << "      name.connect  [ip] [port] [alive]" << endl;
					Serial << "      name.[un]subscribe [topic]" << endl;
					Serial << "      name.publish [topic][payload]" << endl;
					Serial << "      name.view" << endl;
					Serial << "      name.delete" << endl;

					automatic::help();
					Serial << endl;
					Serial << "    help" << endl;
					Serial << "    blink [Dx on_ms off_ms]" << endl;
					Serial << "    ls / ip / reset" << endl;
					Serial << "    set [name][value]" << endl;
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

				if (retval != MqttOk)
				{
					Serial << "## ERROR " << retval << endl;
				}
			}
		}
		else
		{
			cmd=cmd+c;
		}
	}
}
