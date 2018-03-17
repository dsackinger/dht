//////////////////////////////////////////////////////////////////////////
// TcpConnection.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//

#include "TcpConnection.h"

TcpConnection::TcpConnection(asio::ip::tcp::socket socket)
  : socket_(std::move(socket))
{
}

TcpConnection::TcpConnection(
  asio::io_service& io_service,
  const std::string& address, const std::string& port)
  : socket_(io_service)
{
  asio::ip::tcp::resolver resolver(io_service);
  asio::connect(socket_, resolver.resolve({ address, port }));
}

TcpConnection::~TcpConnection()
{
}


void TcpConnection::start()
{

}