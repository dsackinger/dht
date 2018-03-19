//////////////////////////////////////////////////////////////////////////
// HashTable.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the HashTable class

#include "CallClient.h"

#include "protobuf/dht.pb.h"

// Utility functions for building messages and sending them
void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id, bool success)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(success);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id, bool success, bool value)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(success);
  ack->set_bool_value(value);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id, bool success, const std::string& value)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(success);
  ack->set_string_value(value);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}


void CallClient::send_set(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, const std::string& key, const std::string& value, std::uint64_t nodeid)
{
  dht::Msg msg;
  msg.set_id(callid);
  if (nodeid)
    msg.set_node_id(nodeid);
  auto sm = msg.mutable_set_msg();
  sm->set_key(key);
  sm->set_value(value);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_has(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, const std::string& key, std::uint64_t nodeid)
{
  dht::Msg msg;
  msg.set_id(callid);
  if (nodeid)
    msg.set_node_id(nodeid);
  auto hm = msg.mutable_has_msg();
  hm->set_key(key);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_get(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, const std::string& key, std::uint64_t nodeid)
{
  dht::Msg msg;
  msg.set_id(callid);
  if (nodeid)
    msg.set_node_id(nodeid);
  auto gm = msg.mutable_get_msg();
  gm->set_key(key);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_diag(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, std::uint64_t nodeid)
{
  dht::Msg msg;
  msg.set_id(callid);
  if (nodeid)
    msg.set_node_id(nodeid);
  auto dm = msg.mutable_diag_msg();

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}


