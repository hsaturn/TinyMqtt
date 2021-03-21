# TinyMqtt
ESP 8266 is a very capable and small Mqtt Broker and Client

## Features

- Act as as a mqtt broker and/or a mqtt client
- Mqtt 3.1.1 / Qos 0 supported
- Standalone (can work without WiFi) (degraded/local mode)
- Brokers can connect to another broker and becomes then a
  proxy for clients that are connected to it.
- zeroconf, this is a strange but very powerful mode where
  all brokers tries to connect together on the same local network.

## Quickstart

- install Streaming library
- install TinyMqtt library
- modify <TinyMqtt/src/my_credentials.h> (wifi setup)

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
In Zeroconf mode, each ESP is a a broker and scans the local network.
After a while one ESP naturally becomes a 'master' and all ESP are connected together.
No problem if the master dies, a new master will be choosen soon.

## License
Gnu GPL 3.0, see [LICENSE](https://github.com/hsaturn/TinyMqtt/blob/main/LICENSE).
