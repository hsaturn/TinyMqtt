# TinyMqtt

![](https://img.shields.io/github/v/release/hsaturn/TinyMqtt)
![](https://img.shields.io/github/issues/hsaturn/TinyMqtt)
![](https://img.shields.io/badge/paltform-ESP8266-green)
![](https://img.shields.io/github/license/hsaturn/TinyMqtt)
![](https://img.shields.io/badge/Mqtt-%203.1.1-yellow)

ESP 8266 is a small, fast and capable Mqtt Broker and Client

## Features

- Very (very !!) fast broker I saw it re-sent 1000 topics per second for two
  clients that had subscribed (payload ~15 bytes). No topic lost.
  The max I've seen was 2k msg/s (1 client 1 subscription)
- Act as as a mqtt broker and/or a mqtt client
- Mqtt 3.1.1 / Qos 0 supported
- Standalone (can work without WiFi) (degraded/local mode)
- Brokers can connect to another broker and becomes then a
  proxy for clients that are connected to it.
- zeroconf, this is a strange but very powerful mode where
  all brokers tries to connect together on the same local network.

## TODO List
* Use [Async library](https://github.com/me-no-dev/ESPAsyncTCP)
* Implement zeroconf mode (needs async)
* Add a max_clients in MqttBroker. Used with zeroconf, there will be
no need for having tons of clients (also RAM is the problem with many clients)
* Test what is the real max number of clients for broker. As far as I saw, 1k is needed per client which would make more than 30 clients critical.
* ~~MqttMessage uses a buffer 256 bytes which is usually far than needed.~~
* ~~MqttClient does not support more than one subscription at time~~
* ~~MqttClient auto re-subscribe (::resubscribe works bad on broker.emqx.io)~~
* MqttClient auto reconnection
* ~~MqttClient unsubscribe~~
* MqttClient does not sent payload to callback...
* MqttClient user/password
* Wildcards (I may implement only # as I'm not interrested by a clever and cpu consuming matching)

## Quickstart

* install [TinyMqtt library](https://github.com/hsaturn/TinyMqtt)
* modify <libraries/TinyMqtt/src/my_credentials.h> (wifi setup)

## Examples


| Example             | Description                                |
| ---------------------------- | --------------------------------- |
| client-without-wifi | standalone example                         |
| simple-client       | Connect the ESP to an external Mqtt broker |
| simple-broker       | Simple Mqtt broker with your ESP           |
| tinymqtt-test       | Complex console example                    |

- tinymqtt-test : This is a complex sketch with a terminal console
  that allows to add clients publish, connect etc with interpreted commands.

## Standalone mode (zeroconf)
-> The zeroconf mode is not yet implemented
zerofonf clients to connect to broker on local network.

In Zeroconf mode, each ESP is a a broker and scans the local network.
After a while one ESP naturally becomes a 'master' and all ESP are connected together.
No problem if the master dies, a new master will be choosen soon.

## License
Gnu GPL 3.0, see [LICENSE](https://github.com/hsaturn/TinyMqtt/blob/main/LICENSE).
