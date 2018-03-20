//////////////////////////////////////////////////////////////////////////
// TcpListener.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// TcpListener class:
//  Begins listening on a TCP/IP socket for incoming connections.
//

#if !defined(__TCP_LISTENER_H__)
#define __TCP_LISTENER_H__

#include "IConnectionListener.h"
#include "Logger.h"

#include <asio.hpp>

class TcpListener : public std::enable_shared_from_this<TcpListener>
{
public:
  // Listen on a specific address
  TcpListener(
    asio::io_service& io_service,
    const std::string& address, const std::string& port,
    std::shared_ptr<IConnectionListener> listener,
    Logger& log);

  // Listen on a specific endpoint - Requires tcp
  TcpListener(
    asio::io_service& io_service,
    asio::ip::tcp::endpoint endpoint,
    std::shared_ptr<IConnectionListener> listener,
    Logger& log);

  virtual ~TcpListener();

public:
  std::string get_name();
  void start();

private:
  void start_accept();

private:
  asio::io_service& io_service_;
  asio::ip::tcp::acceptor acceptor_;
  asio::ip::tcp::socket socket_;

  std::weak_ptr<IConnectionListener> weak_listener_;

  Logger& log_;
};

#endif // #if !defined(__TCP_LISTENER_H__)

