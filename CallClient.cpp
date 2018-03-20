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
void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(true);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id, bool value)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(true);
  ack->set_bool_value(value);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id, const std::string& value)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(true);
  ack->set_string_value(value);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_ack(std::shared_ptr<TcpConnection> connection, int id, uint64_t value)
{
  dht::Msg msg;
  msg.set_id(0);
  auto ack(msg.mutable_ack_msg());
  ack->set_callid(static_cast<google::protobuf::uint32>(id));
  ack->set_success(true);
  ack->set_uint64_value(value);

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

void CallClient::send_diag(
  std::shared_ptr<TcpConnection> connection,
  std::uint32_t callid,
  std::uint64_t nodeid)
{
  dht::Msg msg;
  msg.set_id(callid);
  if (nodeid)
    msg.set_node_id(nodeid);

  auto dm = msg.mutable_diag_msg();

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

// Client -> Server: I want to join, here is my ID
void CallClient::send_join(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, std::uint64_t newid, std::string address)
{
  dht::Msg msg;
  msg.set_id(callid);

  auto jm = msg.mutable_join_msg();
  jm->set_id(newid);
  jm->set_address(address);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

// Server -> Client : You are allowed to join.  Here is the new node table
void CallClient::send_join_accept(
  std::shared_ptr<TcpConnection> connection,
  std::uint32_t callid,
  std::uint64_t acceptor_id,
  const std::vector <node_info_t>& nodes)
{
  dht::Msg msg;
  msg.set_id(callid);

  auto jam = msg.mutable_join_accept_msg();
  jam->set_acceptor_id(acceptor_id);

  for (auto& entry : nodes)
  {
    auto node = jam->add_nodes();
    node->set_id(entry.id);
    node->set_address(entry.address);
  }

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_join_notify(
  std::shared_ptr<TcpConnection> connection,
  std::uint32_t callid,
  std::uint64_t acceptor_id,
  const std::vector <node_info_t>& nodes)
{
  dht::Msg msg;
  msg.set_id(callid);

  auto jn = msg.mutable_join_notify_msg();
  jn->set_acceptor_id(acceptor_id);
  for (auto& entry : nodes)
  {
    auto node = jn->add_nodes();
    node->set_id(entry.id);
    node->set_address(entry.address);
  }

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

void CallClient::send_join_node_id(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, node_info_t nodeinfo)
{
  dht::Msg msg;
  msg.set_id(callid);

  auto jnid = msg.mutable_join_node_id_msg();
  jnid->set_id(nodeinfo.id);
  jnid->set_address(nodeinfo.address);

  auto buffer = msg.SerializeAsString();
  connection->write(buffer);
}

