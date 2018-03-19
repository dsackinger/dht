//////////////////////////////////////////////////////////////////////////
// TcpConnection.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//

#include "TcpConnection.h"

static constexpr std::size_t buffer_size_sc = 8192;

TcpConnection::TcpConnection(
  asio::io_service& io_service,
  asio::ip::tcp::socket socket,
  Logger& log)
  : io_service_(io_service)
  , socket_(std::move(socket))
  , buffer_(buffer_size_sc)
  , log_(log)
  , listener_()
{
  auto endpoint = socket_.remote_endpoint();
  address_ = endpoint.address().to_string();
  port_ = std::to_string(endpoint.port());
  log_.log("Remote Connection: ", address_, ":", port_);
}

TcpConnection::TcpConnection(
  asio::io_service& io_service,
  const std::string& address, const std::string& port,
  Logger& log)
  : address_(address)
  , port_(port)
  , io_service_(io_service)
  , socket_(io_service)
  , buffer_(buffer_size_sc)
  , log_(log)
{
  asio::ip::tcp::resolver resolver(io_service);

  log_.log("Connecting to: ", address_, ":", port_);
  asio::connect(socket_, resolver.resolve({ address_, port_ }));
}

TcpConnection::~TcpConnection()
{
}

void TcpConnection::start()
{
  start_read();
}

void TcpConnection::close()
{
  if (!socket_.is_open())
    return;

  log_.log("Connection [", socket_.remote_endpoint(), "] closing.");
  socket_.close();
}

void TcpConnection::write(const std::string& message)
{
  auto self(shared_from_this());
  io_service_.post([this, self, message]()
  {
    bool in_progress = !messages_.empty();
    messages_.push_back(message);
    if (!in_progress)
      start_write();
  });
}


void TcpConnection::start_read()
{
  // Need a self pointer to lock us in memory so that
  // the buffer doesn't go away
  auto weak_self(weak_from_this());
  asio::async_read(socket_, asio::buffer(reinterpret_cast<char *>(&read_length_), sizeof(read_length_)),
    [this, weak_self](std::error_code ec, std::size_t length)
  {
    auto self(weak_self.lock());
    if (!self)
    {
      // We just aren't our self today.  ;)  Connection must be gone.
      return;
    }

    if (!ec)
    {
      // Switch back our endian if necessary
      read_length_ = asio::detail::socket_ops::network_to_host_short(read_length_);
      if (read_length_ > buffer_.capacity())
      {
        log_.log("Message size of [", read_length_, "] will not fit in our buffer of capacity: ", buffer_.capacity());
        return;
      }

      // Read the body
      asio::async_read(socket_, asio::buffer(buffer_.data(), read_length_),
        [this, weak_self](std::error_code ec, std::size_t length)
      {
        auto self(weak_self.lock());
        if (!self)
          return;

        if (!ec)
        {
          // Received the body
          auto listener = listener_.lock();

          // If the listener is gone, throw the data on the floor
          if (!listener)
            return;

          listener->incoming_message(self, buffer_, length);
        }
        else
        {
          handle_disconnect();
          return;
        }

        // Trigger the next read
        start_read();
      });
    }
    else
    {
      handle_disconnect();
      return;
    }
  });
}

void TcpConnection::start_write()
{
  if (messages_.empty())
    return;

  auto weak_self(weak_from_this());
  write_length_ = static_cast<uint32_t>(messages_.front().capacity());
  write_length_ = asio::detail::socket_ops::host_to_network_short(write_length_);
  asio::async_write(socket_, asio::buffer(reinterpret_cast<char*>(&write_length_), sizeof(write_length_)),
    [this, weak_self](std::error_code ec, std::size_t length)
  {
    auto self(weak_self.lock());

    if (!self)
      return;

    if (!ec)
    {
      // Data was sent successfully.  See if we need to continue.
      asio::async_write(socket_, asio::buffer(messages_.front().data(), messages_.front().capacity()),
        [this, weak_self](std::error_code ec, std::size_t length)
      {
        auto self(weak_self.lock());
        if (!self)
          return;

        if (!ec)
        {
          // Data was sent successfully.  See if we need to continue.
          messages_.pop_front();
          if (!messages_.empty())
            start_write();
        }
        else
        {
          handle_disconnect();
          socket_.close();
        }
      });
    }
    else
    {
      handle_disconnect();
      socket_.close();
    }
  });
}

void TcpConnection::handle_disconnect()
{
  auto listener = listener_.lock();

  if (!listener)
  {
    log_.log("Disconnect with no listener: ", address_, ":", port_);
    return;
  }

  listener->connection_closing(shared_from_this());
}

