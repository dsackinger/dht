//////////////////////////////////////////////////////////////////////////
// Client.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Client class:
//  This class is for making a connection to a Server (node)
//
//  It starts by establishing the connection and then
//  provides functions to make requests of the Server
//

#if !defined(__CLIENT_H__)
#define __CLIENT_H__

#include "CallManager.h"
#include "Logger.h"
#include "TcpConnection.h"
#include "ThreadPool.h"

#include <asio.hpp>

#include <string>

class Client
  : public IMessageListener
  , public std::enable_shared_from_this<Client>
{
public:
  Client(
    const std::string& address,
    const std::string& port,
    Logger& log);
  virtual ~Client();

public:
  void start();

  bool set_value(const std::string& key, const std::string& value);
  bool has_value(const std::string& key);
  bool get_value(const std::string& key, std::string& value_out);

  bool send_diag();

public:
  // IMessageListener interface
  void incoming_message(
    std::shared_ptr<TcpConnection> connection,
    std::vector<char>& buffer,
    std::size_t length) override;

  void connection_closing(std::shared_ptr<TcpConnection> connection) override;

private:
  // Message Handlers
  void on_msg_ack(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);

private:
  ThreadPool pool_;
  std::shared_ptr<TcpConnection> connection_;

  // For matching up incoming acks to outbound calls
  std::shared_ptr<CallManager> calls_;

  Logger& log_;
};

#endif // #if !defined(__CLIENT_H__)
