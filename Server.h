//////////////////////////////////////////////////////////////////////////
// Server.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Server class:
//  This class implements the Server (node) functionality
//

#if !defined(__SERVER_H__)
#define __SERVER_H__

#include "CallClient.h"
#include "CallManager.h"
#include "HashTable.h"
#include "Logger.h"
#include "TcpConnection.h"
#include "TcpListener.h"
#include "ThreadPool.h"

#include "protobuf/dht.pb.h"

#include <map>

class Server
  : public IConnectionListener
  , public IMessageListener
  , public std::enable_shared_from_this<Server>
{
public:
  Server(Logger& log);
  virtual ~Server();

public:
  void start(std::string& address, std::string& port);
  bool is_exiting() { return exiting_; };
  void stop();
  void join(std::string& address, std::string& port);

public:
  inline asio::io_service& get_io_service() { return pool_->get_io_service(); };

public:
  // IConnectionListener interface
  void incoming_tcp_connection(std::shared_ptr<TcpConnection> connection) override;

public:
  // IMessageListener interface
  void incoming_message(
    std::shared_ptr<TcpConnection> connection,
    std::vector<char>& buffer,
    std::size_t length);

  void connection_closing(std::shared_ptr<TcpConnection> connection) override;

private:
  // Message Handlers
  void on_msg_ack(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);

  void on_msg_set(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);

  void on_msg_has(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);
  bool on_msg_has_ask_remote(const std::string& key, std::shared_ptr<HashTable> table, CallManager::callback_t callback);

  void on_msg_get(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);
  bool on_msg_get_ask_remote(const std::string& key, std::shared_ptr<HashTable> table, CallManager::callback_t callback);

  void on_msg_diag(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);

  void on_msg_join(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);
  void on_msg_join_accept(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);

  void on_msg_join_node_id(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);
  void on_msg_join_notify(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg);

private:
  void diag_notify_nodes(std::uint64_t target);
  void join_accept_node(std::shared_ptr<TcpConnection> connection, std::uint64_t joinid);
  void join_notify_nodes(std::shared_ptr<TcpConnection> connection, std::uint32_t caller_callid, std::uint64_t current, std::uint64_t joinid);
  void join_promote_connection_to_node(std::shared_ptr<TcpConnection> connection, std::uint64_t joinid, std::string address);
  void join_distribute_old_table();

private:
  // Utility functions
  uint64_t get_next_node_id(uint64_t id = 0);
  std::shared_ptr<TcpConnection> get_node_connection(std::uint64_t nodeid);
  std::shared_ptr<TcpConnection> get_client_connection(std::string name);
  void dump_cache();
  void dump_cache_table(const std::string& message, std::shared_ptr<HashTable> table);
  std::vector<CallClient::node_info_t> get_node_info_list();

private:
  std::shared_ptr<ThreadPool> pool_;
  std::atomic<bool> exiting_;
  std::atomic<bool> in_join_;

  // This is our ID that we use on the ring
  std::uint64_t id_;

  // We have at most two hash tables.  The current one and one we are migrating from
  std::shared_ptr<HashTable> current_table_;
  std::shared_ptr<HashTable> old_table_;

  // For matching up incoming acks to outbound calls
  std::shared_ptr<CallManager> calls_;

  // Background thread
  typedef asio::basic_waitable_timer<std::chrono::system_clock> timer_t;
  std::shared_ptr<timer_t> prune_task_timer_;
  void start_background_client_prune_task();

  typedef struct
  {
    std::shared_ptr<TcpConnection> connection;
    std::chrono::system_clock::time_point established;
    std::chrono::system_clock::time_point last;
  } client_connection_t;

  std::mutex connection_lock_;
  typedef std::map<std::string, client_connection_t> client_connection_map_t;
  client_connection_map_t clients_;

  std::mutex node_lock_;
  typedef std::map<uint64_t, std::shared_ptr<TcpConnection>> node_connection_map_t;
  node_connection_map_t nodes_;
  typedef std::map<uint64_t, std::string> node_address_map_t;
  node_address_map_t node_addresses_;

  std::shared_ptr<TcpListener> listener_;

  Logger& log_;
};

#endif // #if !defined(__SERVER_H__)

