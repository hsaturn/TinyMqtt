# vim: noexpandtab


APP_NAME := compile-tests
ARDUINO_LIBS := AUnit AceCommon AceTime TinyMqtt EspMock ESP8266WiFi  ESPAsync ESP8266mDNS TinyConsole
ARDUINO_LIB_DIRS := ../../../EspMock/libraries
EPOXY_CORE := EPOXY_CORE_ESP8266
DUMMY := 1
RUN = no
APP_SRCS_COMP += $(shell find ../../examples -name "*.ino")
APP_SRCS_CPP += $(APP_SRCS_COMP:%.ino=%.o)
CXXFLAGS = -D_GNU_SOURCE -Werror=return-type -std=gnu++17 -Wall -g3 -O0 -DEPOXY_DUINO

include ../Makefile.opts

all:

compile-tests.out: $(OBJS)

run:
	@echo "No run for compile tests"
	exit 0

