# TinyMqtt

[![Release](https://img.shields.io/github/v/release/hsaturn/TinyMqtt)](https://github.com/hsaturn/TinyMqtt/releases)
[![AUnit Tests](https://github.com/hsaturn/TinyMqtt/actions/workflows/aunit.yml/badge.svg)](https://github.com/hsaturn/TinyMqtt/actions/workflows/aunit.yml)
[![Issues](https://img.shields.io/github/issues/hsaturn/TinyMqtt)](https://github.com/hsaturn/TinyMqtt/issues)
[![Esp8266](https://img.shields.io/badge/platform-ESP8266-green)](https://www.espressif.com/en/products/socs/esp8266)
[![Esp32](https://img.shields.io/badge/platform-ESP32-green)](https://www.espressif.com/en/products/socs/esp32)
[![Gpl 3.0](https://img.shields.io/github/license/hsaturn/TinyMqtt)](https://www.gnu.org/licenses/gpl-3.0.fr.html)
[![Mqtt 3.1.1](https://img.shields.io/badge/Mqtt-%203.1.1-yellow)](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html#_Toc442180822)

TinyMqtt is a small, fast and capable Mqtt Broker and Client for Esp8266 / Esp32 / Esp WROOM

This is an older version (the latest one from the Arduino library manager) modified to allow usage with a W5500 LAN adapter.

### Statuses of all unit tests of TinyMqtt and its dependencies

| Project     | Unit tests result |
| ----------- | ------------ |
| TinyMqtt    | [![](https://github.com/hsaturn/TinyMqtt/actions/workflows/aunit.yml/badge.svg)](https://github.com/hsaturn/TinyMqtt/actions/workflows/aunit.yml) |
| Dependencies                  ||
| TinyConsole | [![](https://github.com/hsaturn/TinyConsole/actions/workflows/aunit.yml/badge.svg)](https://github.com/hsaturn/TinyConsole/actions/workflows/aunit.yml) |
| EpoxyDuino  | [![AUnit Tests](https://github.com/hsaturn/EpoxyDuino/actions/workflows/aunit_tests.yml/badge.svg)](https://github.com/hsaturn/EpoxyDuino/actions/workflows/aunit_tests.yml) |
| EspMock     | [![AUnit Tests](https://github.com/hsaturn/EspMock/actions/workflows/aunit.yml/badge.svg)](https://github.com/hsaturn/EspMock/actions/workflows/aunit.yml) |
| AUnit | [![AUnit Tests](https://github.com/hsaturn/AUnit/actions/workflows/aunit_tests.yml/badge.svg)](https://github.com/hsaturn/AUnit/actions/workflows/aunit_tests.yml) |
| AceRoutine | [![AUnit Tests](https://github.com/bxparks/AceRoutine/actions/workflows/aunit_tests.yml/badge.svg)](https://github.com/bxparks/AceRoutine/actions/workflows/aunit_tests.yml) |

## Features

- Supports retained messages (not activated by default)
- Async Wifi compatible (me-no-dev/ESPAsyncTCP@^1.2.2)
- Very fast broker I saw it re-sent 1000 topics per second for two
  clients that had subscribed (payload ~15 bytes ESP8266). No topic lost.
  The max I've seen was 2k msg/s (1 client 1 subscription)
- Act as as a mqtt broker and/or a mqtt client
- Mqtt 3.1.1 / Qos 0 supported
- Wildcards supported (+ # $ and * (even if not part of the spec...))
- Standalone (can work without WiFi) (degraded/local mode)
- Brokers can connect to another broker and becomes then a
  proxy for clients that are connected to it.
- zeroconf, this is a strange but very powerful mode where
  all brokers tries to connect together on the same local network.
- small memory footprint (very efficient topic storage)
- long messages are supported (>127 bytes)
- TinyMQTT is largely unit tested, so once a bug is fixed, it is fixed forever

## Limitations

- Max of 255 different topics can be stored (change index_t type to allow more)
- No Qos because messages are not queued but immediately sent to clients

## Quickstart

* install [TinyMqtt library](https://github.com/hsaturn/TinyMqtt)
  (you can use the Arduino library manager and search for TinyMqtt)
* modify <libraries/TinyMqtt/src/my_credentials.h> (wifi setup)

## Examples

| Example             | Description                                |
| ------------------- | ------------------------------------------ |
| [client-with-wifi](https://github.com/hsaturn/TinyMqtt/tree/main/examples/client-with-wifi/client-with-wifi.ino) | standalone example                         |
| [client-without-wifi](https://github.com/hsaturn/TinyMqtt/tree/main/examples/client-without-wifi/client-without-wifi.ino) | standalone example                         |
| [simple-client](https://github.com/hsaturn/TinyMqtt/tree/main/examples/simple-client/simple-client.ino)       | Connect the ESP to an external Mqtt broker |
| [broker-W5500](https://github.com/real-bombinho/TinyMqtt/tree/main/examples/client-with-W5500)       | simple Mqtt broker for ESP8266 + W55500 |
| [simple-broker](https://github.com/hsaturn/TinyMqtt/tree/main/examples/simple-broker/simple-broker.ino)       | Simple Mqtt broker with your ESP           |
| [tinymqtt-test](https://github.com/hsaturn/TinyMqtt/tree/main/examples/tinymqtt-test/tinymqtt-test.ino)       | Complex console example                    |

- tinymqtt-test : This is a complex sketch with a terminal console
  that allows to add clients publish, connect etc with interpreted commands.

## Retained messages

Qos 1 is not supported, but retained messages are. So a new subscription is able to send old messages.
This feature is disabled by default.
The default retain parameter of MqttBroker::MqttBroker takes an optional (0 by default) number of retained messages.
MqttBroker::retain(n) will also make the broker store n messages at max.

## Standalone mode (zeroconf)
-> The zeroconf mode is not yet implemented
zeroconf clients to connect to broker on local network.

In Zeroconf mode, each ESP is a a broker and scans the local network.
After a while one ESP naturally becomes a 'master' and all ESP are connected together.
No problem if the master dies, a new master will be choosen soon.

## TODO List
* ~~Use [Async library](https://github.com/me-no-dev/ESPAsyncTCP)~~
* Implement zeroconf mode (needs async)
* Add a max_clients in MqttBroker. Used with zeroconf, there will be
no need for having tons of clients (also RAM is the problem with many clients)
* Why not a 'global' TinyMqtt::loop() instead of having to call loop for all broker/clients instances
* Test what is the real max number of clients for broker. As far as I saw, 1k is needed per client which would make more than 30 clients critical.
* ~~MqttClient auto re-subscribe (::resubscribe works bad on broker.emqx.io)~~
* MqttClient auto reconnection
* MqttClient user/password
* ~~Wildcards (I may implement only # as I'm not interrested by a clever and cpu consuming matching)~~
* I suspect that MqttClient::parent could be removed and replaced with a simple boolean
  (this'll need to rewrite a few functions)

## License
Gnu GPL 3.0, see [LICENSE](https://github.com/hsaturn/TinyMqtt/blob/main/LICENSE).
