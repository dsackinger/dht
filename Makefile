CC=gcc
CXX=g++
RM=rm -f
MKDIR=mkdir -p
PROTOC=protoc
PROTOCFLAGS=--cpp_out=protobuf/
CPPFLAGS=-std=c++11 -DASIO_STANDALONE -I../asio/asio/include
LDFLAGS=-g
LDLIBS=-lpthread -lprotobuf

SRCS = \
 main.cpp \
 CallClient.cpp \
 CallManager.cpp \
 Client.cpp \
 HashTable.cpp \
 Logger.cpp \
 Server.cpp \
 TcpConnection.cpp \
 TcpListener.cpp \
 ThreadPool.cpp

OBJS=$(subst .cpp,.o,$(SRCS)) protobuf/dht.pb.o
PBS = dht.proto

all: dht

protobuf/dht.pb.cc: dht.proto
	$(PROTOC) $(PROTOCFLAGS) $^

dht: $(OBJS)
	$(CXX) $(LDFLAGS) -o dht $(OBJS) $(LDLIBS)

depend: .depend

.depend: $(SRCS) protobuf/dht.pb.cc
	$(RM) ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>> ./.depend;

clean:
	$(RM) protobuf/*
	$(RM) *.o
	$(RM) *~ .depend

include .depend

