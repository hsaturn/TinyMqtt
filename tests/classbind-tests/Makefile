# See https://github.com/bxparks/EpoxyDuino for documentation about this
# Makefile to compile and run Arduino programs natively on Linux or MacOS.

EXTRA_CXXFLAGS=-g3 -O0 -DTINY_MQTT_TESTS

APP_NAME := classbind-tests
ARDUINO_LIBS := AUnit AceCommon AceTime TinyMqtt EspMock ESP8266WiFi  ESPAsyncTCP TinyConsole
ARDUINO_LIB_DIRS := ../../../EspMock/libraries
EPOXY_CORE := EPOXY_CORE_ESP8266
include ../../../EpoxyDuino/EpoxyDuino.mk
