//////////////////////////////////////////////////////////////////////////
// CallClient.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Call Client:
//  Utility functions for sending calls on a connection
//

#if !defined(__CALL_CLIENT_H__)
#define __CALL_CLIENT_H__

#include "TcpConnection.h"

class CallClient
{
private:
  CallClient() {};
  virtual ~CallClient() {};

public:
  // Ack Messages
  static void send_ack(std::shared_ptr<TcpConnection> connection, int id, bool success);
  static void send_ack(std::shared_ptr<TcpConnection> connection, int id, bool success, bool value);
  static void send_ack(std::shared_ptr<TcpConnection> connection, int id, bool success, const std::string& value);

  // Set Messages
  static void send_set(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, const std::string& key, const std::string& value, std::uint64_t nodeid = 0);

  // Has Messages
  static void send_has(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, const std::string& key, std::uint64_t nodeid = 0);

  // Get Messages
  static void send_get(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, const std::string& key, std::uint64_t nodeid = 0);

  // Diag Messages
  static void send_diag(std::shared_ptr<TcpConnection> connection, std::uint32_t callid, std::uint64_t nodeid = 0);
};

#endif // #if !defined(__CALL_CLIENT_H__)

