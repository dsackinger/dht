//////////////////////////////////////////////////////////////////////////
// TcpListener.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// TcpListener implementation
//

#include "TcpListener.h"

TcpListener::TcpListener(
  asio::io_service& io_service,
  const std::string& address, const std::string& port,
  std::shared_ptr<IConnectionListener> listener,
  Logger& log)
  : io_service_(io_service)
  , acceptor_(io_service_)
  , socket_(io_service_)
  , weak_listener_(listener)
  , log_(log)
{
  asio::ip::tcp::resolver resolver(io_service_);
  asio::ip::tcp::endpoint endpoint = *resolver.resolve({ address, port });
  log_.log("Listening on ", endpoint);

  acceptor_.open(endpoint.protocol());
  acceptor_.bind(endpoint);
  acceptor_.listen();
}

TcpListener::TcpListener(
  asio::io_service& io_service,
  asio::ip::tcp::endpoint endpoint,
  std::shared_ptr<IConnectionListener> listener,
  Logger& log)
  : io_service_(io_service)
  , acceptor_(io_service_)
  , socket_(io_service)
  , weak_listener_(listener)
  , log_(log)
{
  log_.log("Listening on ", endpoint);

  acceptor_.open(endpoint.protocol());
  acceptor_.bind(endpoint);
  acceptor_.listen();
}

TcpListener::~TcpListener()
{
  acceptor_.close();
}

void TcpListener::start()
{
  // Must be called outside of the constructor so the shared this is established.
  start_accept();
}

void TcpListener::start_accept()
{
  auto weak_self(weak_from_this());
  acceptor_.async_accept(socket_,
    [this, weak_self](std::error_code ec)
  {
    auto self = weak_self.lock();
    if (!self)
      return;

    if (!ec)
    {
      auto listener(weak_listener_.lock());
      if (!listener)
        return;

      auto connection = std::make_shared<TcpConnection>(io_service_, std::move(socket_), log_);
      listener->incoming_tcp_connection(connection);
    }

    start_accept();
  });
}

