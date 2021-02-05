#
# Makefile
#
# Makefile for Paho MQTT Client
#
# This makefile is not used to build Paho inside Machina, but it can be
# handy for testing of update.
#

include $(POCO_BASE)/build/rules/global

SYSLIBS += -lssl -lcrypto

ifeq ($(OSNAME),Darwin)
CFLAGS += -DOSX -Wno-deprecated-declarations
endif

CFLAGS += -DOPENSSL

objects = \
	Base64
	Clients \
	Heap \
	LinkedList \
	Log \
	MQTTAsyncUtils
    MQTTClient \
	MQTTPacket \
	MQTTPacketOut \
	MQTTPersistence \
	MQTTPersistenceDefault \
	MQTTProtocolClient \
	MQTTProperties \
	MQTTProtocolOut \
	MQTTReasonCodes \
	MQTTTime \
	MQTTVersion \
	Messages \
	OsWrapper \
	SHA1 \
	SSLSocket \
	Socket \
	SocketBuffer \
	StackTrace \
	Thread \
	WebSocket \
	Tree \
	utf-8

target         = PahoMQTT
target_version = 1
target_libs    =

include $(POCO_BASE)/build/rules/lib
