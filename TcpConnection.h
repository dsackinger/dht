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

#include "IMessageListener.h"
#include "Logger.h"

#include <asio.hpp>

#include <cstdint>
#include <deque>
#include <memory>

class TcpConnection
  : public std::enable_shared_from_this<TcpConnection>
{
public:
  TcpConnection(
    asio::io_service& io_service,
    asio::ip::tcp::socket socket,
    Logger& log);

  TcpConnection(
    asio::io_service& io_service,
    const std::string& address,
    const std::string& port,
    Logger& log);

  virtual ~TcpConnection();

public:
  inline void set_listener(std::shared_ptr<IMessageListener> listener) { listener_ = listener; };

  std::string get_connection_name();

  void start();
  void close();

  void write(const std::string& message);

private:
  void start_read();
  void start_write();

  void handle_disconnect();

private:
  std::string address_;
  std::string port_;

  asio::io_service& io_service_;
  asio::ip::tcp::socket socket_;
  asio::detail::u_short_type read_length_;
  asio::detail::u_short_type write_length_;
  std::vector<char> buffer_;

  std::weak_ptr<IMessageListener> listener_;

  typedef std::deque<std::string> message_queue_t;
  message_queue_t messages_;

  Logger& log_;

private:
  // Access control
  TcpConnection(const TcpConnection&) = delete;
  TcpConnection& operator=(const TcpConnection&) = delete;
};

#endif // #if !defined(__TCP_CONNECTION_H__)

