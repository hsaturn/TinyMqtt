// MqttReceiver must implement onPublish(...)
template <class MqttReceiver>
class MqttClassBinder
{
  public:
    MqttClassBinder()
    {
      unregister(this);
    }
    ~MqttClassBinder() { unregister(this); }

    static void onUnpublished(MqttClient::CallBack handler)
    {
      unrouted_handler = handler;
    }

    static void onPublish(MqttClient* client, MqttReceiver* dest)
    {
      routes.insert(std::pair<MqttClient*, MqttReceiver*>(client, dest));
      client->setCallback(onRoutePublish);
    }

    void onPublish(const MqttClient* client, const Topic& topic, const char* payload, size_t length)
    {
      static_cast<MqttReceiver*>(this)->MqttReceiver::onPublish(client, topic, payload, length);
    }

    static size_t size() { return routes.size(); }

    static void reset() { routes.clear(); }

  private:

    static void onRoutePublish(const MqttClient* client, const Topic& topic, const char* payload, size_t length)
    {
      bool unrouted = true;
      auto receivers = routes.equal_range(client);
      for(auto it = receivers.first; it != receivers.second; ++it)
      {
        it->second->onPublish(client, topic, payload, length);
        unrouted = false;
      }

      if (unrouted and unrouted_handler)
      {
        unrouted_handler(client, topic, payload, length);
      }
    }

  private:
    void unregister(MqttClassBinder<MqttReceiver>* which)
    {
      if (routes.size()==0) return;   // bug in map stl
      for(auto it=routes.begin(); it!=routes.end(); it++)
        if (it->second == which)
        {
          routes.erase(it);
          return;
        }
    }

  static std::multimap<const MqttClient*, MqttClassBinder<MqttReceiver>*> routes;
  static MqttClient::CallBack unrouted_handler;

};

template<class MqttReceiver>
std::multimap<const MqttClient*, MqttClassBinder<MqttReceiver>*> MqttClassBinder<MqttReceiver>::routes;

template<class MqttReceiver>
MqttClient::CallBack MqttClassBinder<MqttReceiver>::unrouted_handler = nullptr;

