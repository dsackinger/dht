//////////////////////////////////////////////////////////////////////////
// TcpConnection.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// TcpConnection class:
//  Created by either a TcpListener as an incoming connection
//  or Instantiated as an outgoing connection
//

#if !defined(__TCP_CONNECTION_H__)
#define __TCP_CONNECTION_H__

#include <asio.hpp>

#include <memory>

class TcpConnection
  : public std::enable_shared_from_this<TcpConnection>
{
public:
  TcpConnection(asio::ip::tcp::socket socket);
  TcpConnection(
    asio::io_service& io_service,
    const std::string& address,
    const std::string& port);
  virtual ~TcpConnection();

public:
  void start();

private:
  asio::ip::tcp::socket socket_;
  std::array<char, 8192> buffer_;

private:
  // Access control
  TcpConnection(const TcpConnection&) = delete;
  TcpConnection& operator=(const TcpConnection&) = delete;
};

#endif // #if !defined(__TCP_CONNECTION_H__)

