# TinyMqtt
ESP 8266 is a very capable Mqtt Broker and Client

Here are the features
- mqtt client can Works without WiFi (local mode) in a unique ESP
  Thus, publishes and subscribes are possible and allows
  minimal (degraded) function of a single module.
- broker can connect to another broker and becomes then a
  proxy for clients that are connected to it.
- zeroconf, this is a strange but very powerful mode
  where each ESP is a a broker and scans the local network.
  After a while one ESP becomes the 'master'
  and all ESP are connected together. The master can die
  whithout breaking the system.
