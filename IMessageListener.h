//////////////////////////////////////////////////////////////////////////
// IMessageListener.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// IMessageListener interface:
//  Mechanism for receiving data from connections
//

#if !defined(__IMESSAGE_LISTENER_H__)
#define __IMESSAGE_LISTENER_H__

#include <memory>
#include <vector>

class TcpConnection;

class IMessageListener
{
public:
  IMessageListener() = default;
  virtual ~IMessageListener() = default;

public:
  // IMessageListener interface
  virtual void incoming_message(
    std::shared_ptr<TcpConnection> connection,
    std::vector<char>& buffer,
    std::size_t length) = 0;

  virtual void connection_closing(std::shared_ptr<TcpConnection> connection) = 0;
};

#endif // #if !defined(__IMESSAGE_LISTENER_H__)

