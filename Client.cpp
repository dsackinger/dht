//////////////////////////////////////////////////////////////////////////
// Client.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the Client class

#include "Client.h"
#include "CallClient.h"

Client::Client(
  const std::string& address,
  const std::string& port,
  Logger& log)
  : log_(log)
  , pool_(std::make_shared<ThreadPool>(1, log))
  , calls_(std::make_shared<CallManager>(pool_))
  , connection_(std::make_shared<TcpConnection>(pool_->get_io_service(), address, port, log))
{
}

Client::~Client()
{
}

void Client::start()
{
  pool_->start();
  connection_->set_listener(shared_from_this());
  connection_->start();
}

bool Client::set_value(const std::string& key, const std::string& value)
{
  bool complete = false;
  bool success = false;

  // Build the message to send
  auto callid = calls_->register_callback(
    [this, key, value, &complete, &success](const dht::AckMsg& ack)
  {
    success = ack.success();
    complete = true;
  });

  CallClient::send_set(connection_, callid, key, value);

  while (!complete)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return success;
}

bool Client::has_value(const std::string& key)
{
  bool has = false;
  bool complete = false;

  // Build the message to send
  auto callid = calls_->register_callback(
    [this, key, &has, &complete](const dht::AckMsg& ack)
  {
    if (ack.success() && ack.bool_value())
      has = true;

    complete = true;
  });

  CallClient::send_has(connection_, callid, key);

  while (!complete)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return has;
}

bool Client::get_value(const std::string& key, std::string& value_out)
{
  bool complete = false;
  bool success = false;

  // Build the message to send
  auto callid = calls_->register_callback(
    [this, key, &complete, &success, &value_out](const dht::AckMsg& ack)
  {
    if (ack.success())
      value_out = ack.string_value();

    complete = true;
  });

  CallClient::send_get(connection_, callid, key);

  while (!complete)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return true;
}

bool Client::send_diag()
{
  bool complete = false;
  bool success = false;

  // Build the message to send
  auto callid = calls_->register_callback(
    [this, &complete, &success](const dht::AckMsg& ack)
  {
    success = ack.success();
    complete = true;
  });

  CallClient::send_diag(connection_, callid);

  while (!complete)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return success;
}

// Message Handlers
void Client::on_msg_ack(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto id = msg.id();
  auto callback = calls_->get_callback(id);
  if (!callback)
    log_.log("Received an ack for an unknown callback [", id, "]");
  else
    callback(msg.ack_msg());
  calls_->remove_callback(id);
}

// IMessageListener interface
void Client::incoming_message(
  std::shared_ptr<TcpConnection> connection,
  std::vector<char>& buffer,
  std::size_t length)
{
  dht::Msg msg;
  msg.ParseFromArray(buffer.data(), static_cast<int>(length));

  // Look for responses first
  if (msg.has_ack_msg())
    on_msg_ack(connection, msg);
  else
    log_.log("Received message of unknown type.");
}

void Client::connection_closing(std::shared_ptr<TcpConnection> connection)
{
  log_.log("Connection closed.");
}

