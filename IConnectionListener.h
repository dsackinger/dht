//////////////////////////////////////////////////////////////////////////
// IConnectionListener.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// IConnectionListener interface:
//  Mechanism to decouple owning class from the listener
//

#if !defined(__ICONNECTION_LISTENER_H__)
#define __ICONNECTION_LISTENER_H__

#include "TcpConnection.h"

#include <memory>

class IConnectionListener
{
public:
  IConnectionListener() = default;
  virtual ~IConnectionListener() = default;

public:
  // IConnectionListener interface
  virtual void incoming_tcp_connection(std::shared_ptr<TcpConnection> connection) = 0;
};

#endif // #if !defined(__ICONNECTION_LISTENER_H__)
