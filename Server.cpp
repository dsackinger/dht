//////////////////////////////////////////////////////////////////////////
// Server.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the Server class

#include "Server.h"

#include "CallClient.h"

#include <functional>

Server::Server(Logger& log)
  : log_(log)
  , pool_(1, log)
  , listener_()
  , calls_(std::make_shared<CallManager>(pool_.get_io_service()))
{
  // Calculate a unique ID for us
  auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

  // To make sure the generated ids aren't in order, hash it
  std::hash<long long> hasher;
  id_ = static_cast<std::uint64_t>(hasher(count));

  if (!id_)
  {
    log_.log("Failure - Node came up with ID [0]");
    std::exit(2);
  }

  // Now set up our current hash table with only us in it
  auto nodes = std::vector<std::uint64_t>({ id_ });
  current_table_ = std::make_shared<HashTable>(0, nodes);
}

Server::~Server()
{
  if (prune_task_timer_)
    prune_task_timer_->cancel();
}


void Server::start(std::string& address, std::string& port)
{
  // The big deal here is to start the listener.  The listener needs to have us
  // as a callback, so can't be instantiated during construction
  if (!address.empty())
  {
    // We are to bind on a specific address
    listener_ = std::make_shared<TcpListener>(pool_.get_io_service(), address, port, shared_from_this(), log_);
  }
  else
  {
    // Pick the default v4 address to bind
    auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), std::atoi(port.c_str()));
    listener_ = std::make_shared<TcpListener>(pool_.get_io_service(), endpoint, shared_from_this(), log_);
  }

  // Now start the listener
  listener_->start();
}

// IConnectionListener interface
void Server::incoming_tcp_connection(std::shared_ptr<TcpConnection> connection)
{
  // We have a new connection.  We don't know anything until it talks to us.  For now, just
  // put him in the client_connections_ structure
  auto endpoint = connection->get_remote_endpoint();
  std::stringstream ss;
  ss << endpoint;
  std::string ep = ss.str();
  log_.log("Incoming connection from :", ep);

  { // Scope for lock
    std::unique_lock<std::mutex> lock(connection_lock_);

    auto now(std::chrono::system_clock::now());
    clients_[ep] = { connection, now, now };
  }
  connection->set_listener(shared_from_this());
  connection->start();
}


// IMessageListener interface
void Server::incoming_message(
  std::shared_ptr<TcpConnection> connection,
  std::vector<char>& buffer,
  std::size_t length)
{
  dht::Msg msg;
  msg.ParseFromArray(buffer.data(), static_cast<int>(length));

  // Look for client requests first
  if (msg.has_ack_msg())
    on_msg_ack(connection, msg);
  else if (msg.has_set_msg())
    on_msg_set(connection, msg);
  else if (msg.has_has_msg())
    on_msg_has(connection, msg);
  else if (msg.has_get_msg())
    on_msg_get(connection, msg);
  else if (msg.has_diag_msg())
    on_msg_diag(connection, msg);
  else
  {
    log_.log("Receive unknown message type.");
    CallClient::send_ack(connection, msg.id(), false);
  }
}

void Server::connection_closing(std::shared_ptr<TcpConnection> connection)
{
  // We have a connection closing.  Do the bookkeeping for this connection
  std::unique_lock<std::mutex> lock(connection_lock_);

  std::vector<std::string> remove_connections;
  for (auto& client : clients_)
  {
    auto& client_connection = client.second;
    if (client_connection.connection == connection)
      remove_connections.push_back(client.first);
  }

  for (auto& name : remove_connections)
  {
    log_.log("Connection closed: ", name);
    clients_.erase(name);
  }

  // If we allowed for disconnecting nodes, we would do it here.
}


// Message Handlers
void Server::on_msg_ack(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto id = msg.id();
  auto callback = calls_->get_callback(id);
  if (!callback)
    log_.log("Received an ack for an unknown callback [", id, "]");
  else
    callback(msg.ack_msg());
  calls_->remove_callback(id);
}

void Server::on_msg_set(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& set_msg = msg.set_msg();
  if (!set_msg.has_key() || !set_msg.has_value())
  {
    log_.log("Incomplete set message received.");
    return;
  }

  auto& key = set_msg.key();
  auto& value = set_msg.value();

  log_.log("Processing set message Key[", key, "] Value[", value, "]");

  // Decide if we should add this to our local cache
  // All incoming sets go in the current_table_
  auto target_node = current_table_->node_from_key(key);
  if (id_ == target_node)
  {
    // Just dump the value in our current table
    current_table_->set(key, value);
    CallClient::send_ack(connection, caller_callid, true);
    return;
  }

  // Proxy this along to the target
  auto node_connection = get_node_connection(target_node);
  if (!node_connection)
  {
    // We didn't find a connection to send this?
    log_.log("Failed to find node connection [", target_node, "]");
    return;
  }

  // Build the message to send
  auto callid = calls_->next_call();
  calls_->register_callback(
    [this, callid, connection, caller_callid](const dht::AckMsg& ack)
  {
    // We just pass back through to the client
    CallClient::send_ack(connection, caller_callid, ack.success() ? ack.bool_value() : false);
  });

  CallClient::send_set(connection, callid, key, value, id_);
}

void Server::on_msg_has(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& has_msg = msg.has_msg();
  if (!has_msg.has_key())
  {
    log_.log("Incomplete has message received.");
    return;
  }

  auto& key = has_msg.key();
  log_.log("Processing has message Key[", key, "]");

  // Check our local table(s) first
  // Check our cache
  if (current_table_->has(key))
  {
    CallClient::send_ack(connection, caller_callid, true, true);
    return;
  }

  // If we have an old table, check that one too
  if (old_table_ && old_table_->has(key))
  {
    CallClient::send_ack(connection, caller_callid, true, true);
    return;
  }

  // See if this call is coming from another node.  If it is coming
  // from another node, we don't allow for forwarding
  if (msg.has_node_id())
  {
    // Not in our cache
    CallClient::send_ack(connection, caller_callid, true, false);
    return;
  }

  // First, create a method for handling the old callback
  auto old_response_callback = 
    [this, connection, caller_callid](const dht::AckMsg& ack)
  {
    bool answer = false;
    if (ack.success() && ack.bool_value())
      answer = true;

    // If we found it, let the caller know
    CallClient::send_ack(connection, caller_callid, true, answer);
  };

  // Try to ask our target
  auto ret = on_msg_has_ask_remote(key, current_table_,
    [this, connection, caller_callid, key, old_response_callback](const dht::AckMsg& ack)
  {
    if (ack.success() && ack.bool_value())
    {
      // Found it in the target's current table
      CallClient::send_ack(connection, caller_callid, true, true);
      return;
    }

    // Fall back to trying our old table
    if (on_msg_has_ask_remote(key, old_table_, old_response_callback))
      return;

    // Couldn't send it... Tell the client we failed
    CallClient::send_ack(connection, caller_callid, true, false);
  });

  // If we successfully sent it, we will have received the callback
  // and processed the second successfully above
  if (ret)
    return;

  // Couldn't send to client on the new table.  Fall back to try
  // one last time on the old_table if we have it
  if (on_msg_has_ask_remote(key, old_table_, old_response_callback))
    return;

  CallClient::send_ack(connection, caller_callid, true, false);
}

bool Server::on_msg_has_ask_remote(
  const std::string& key,
  std::shared_ptr<HashTable> table,
  CallManager::callback_t callback)
{
  // Make sure we have a table
  if (!table)
    return false;

  // Send to our target node
  auto nodeid = table->node_from_key(key);
  if (nodeid == id_)
    return false;

  auto node_connection = get_node_connection(nodeid);
  if (!node_connection)
    return false;

  auto callid = calls_->register_callback(
    [this, callback](const dht::AckMsg& ack)
  {
    // We just pass back through to the client
    callback(ack);
  });

  CallClient::send_has(node_connection, callid, key, id_);
  return true;
}

void Server::on_msg_get(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& get_msg = msg.get_msg();
  if (!get_msg.has_key())
  {
    log_.log("Incomplete get message received.");
    return;
  }

  auto& key = get_msg.key();
  log_.log("Processing get message Key[", key, "]");

  // Check our local table(s) first
  // Check our cache
  std::string value_out;
  if (current_table_->get(key, value_out))
  {
    CallClient::send_ack(connection, caller_callid, true, value_out);
    return;
  }

  // If we have an old table, check that one too
  if (old_table_ && old_table_->get(key, value_out))
  {
    CallClient::send_ack(connection, caller_callid, true, value_out);
    return;
  }

  // See if this call is coming from another node.  If it is coming
  // from another node, we don't allow for forwarding
  if (msg.has_node_id())
  {
    // Not in our cache
    CallClient::send_ack(connection, caller_callid, true);
    return;
  }

  // First, create a method for handling the old callback
  auto old_response_callback =
    [this, connection, caller_callid](const dht::AckMsg& ack)
  {
    if (!ack.success() || !ack.has_string_value())
    {
      // Didn't get a value, don't pass the string_value
      CallClient::send_ack(connection, caller_callid, true);
      return;
    }

    // Send back the value!
    CallClient::send_ack(connection, caller_callid, true, ack.string_value());
  };

  // Try to ask our target
  auto ret = on_msg_get_ask_remote(key, current_table_,
    [this, connection, caller_callid, key, old_response_callback](const dht::AckMsg& ack)
  {
    if (ack.success() && ack.has_string_value())
    {
      // Found it in the target's current table
      CallClient::send_ack(connection, caller_callid, true, ack.string_value());
      return;
    }

    // Fall back to trying our old table
    if (on_msg_get_ask_remote(key, old_table_, old_response_callback))
      return;

    // Couldn't send it... Tell the client we failed
    CallClient::send_ack(connection, caller_callid, true);
  });

  // If we successfully sent it, we will have received the callback
  // and processed the second successfully above
  if (ret)
    return;

  // Couldn't send to client on the new table.  Fall back to try
  // one last time on the old_table if we have it
  if (on_msg_get_ask_remote(key, old_table_, old_response_callback))
    return;

  CallClient::send_ack(connection, caller_callid, true);
}

void Server::on_msg_diag(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& diag_msg = msg.diag_msg();

  log_.log("Processing diag message");
  dump_cache();

  CallClient::send_ack(connection, caller_callid, true);

//   // Decide if we should add this to our local cache
//   // All incoming sets go in the current_table_
//   auto target_node = current_table_->node_from_key(key);
//   if (id_ == target_node)
//   {
//     // Just dump the value in our current table
//     current_table_->set(key, value);
//     CallClient::send_ack(connection, caller_callid, true);
//     return;
//   }
// 
//   // Proxy this along to the target
//   auto node_connection = get_node_connection(target_node);
//   if (!node_connection)
//   {
//     // We didn't find a connection to send this?
//     log_.log("Failed to find node connection [", target_node, "]");
//     return;
//   }
// 
//   // Build the message to send
//   auto callid = calls_->next_call();
//   calls_->register_callback(
//     [this, callid, connection, caller_callid](const dht::AckMsg& ack)
//   {
//     // We just pass back through to the client
//     CallClient::send_ack(connection, caller_callid, ack.success() ? ack.bool_value() : false);
//   });
// 
//  CallClient::send_set(connection, callid, id_, key, value);
}

bool Server::on_msg_get_ask_remote(
  const std::string& key,
  std::shared_ptr<HashTable> table,
  CallManager::callback_t callback)
{
  // Make sure we have a table
  if (!table)
    return false;

  // Send to our target node
  auto nodeid = table->node_from_key(key);
  if (nodeid == id_)
    return false;

  auto node_connection = get_node_connection(nodeid);
  if (!node_connection)
    return false;

  auto callid = calls_->register_callback(
    [this, callback](const dht::AckMsg& ack)
  {
    // We just pass back through to the client
    callback(ack);
  });

  CallClient::send_get(node_connection, callid, key, id_);
  return true;
}


// Utility functions
std::shared_ptr<TcpConnection> Server::get_node_connection(std::uint64_t nodeid)
{
  // Need to lock the node connections
  std::unique_lock<std::mutex> lock(node_lock_);

  auto entry = nodes_.find(nodeid);
  if (entry == nodes_.end())
    return std::shared_ptr<TcpConnection>();

  // Found it... Return the connection
  return entry->second;
}

void Server::dump_cache()
{
  dump_cache_table("Dumping current table:", current_table_);
  dump_cache_table("Dumping old table:", old_table_);
}

void Server::dump_cache_table(const std::string& message, std::shared_ptr<HashTable> table)
{
  if (!table)
  {
    log_.log(message, " table is not allocated");
    return;
  }

  std::stringstream ss;
  ss << message << std::endl;

  auto keys = current_table_->keys();
  ss << "Table has [" << keys.size() << "] values" << std::endl;
  
  bool first = true;
  for (auto& key : keys)
  {
    std::string value_out;

    if (first)
      first = false;
    else
      ss << std::endl;

    ss << "[" << key << "] -> ";
    if (!table->get(key, value_out))
      ss << "Unable to obtain value";
    else
      ss << "[" << value_out << "]";
  }

  log_.log(ss.str().c_str());
}


void Server::start_background_client_prune_task()
{

  prune_task_timer_ = std::make_shared<timer_t>(pool_.get_io_service(), std::chrono::seconds(5));

  auto weak_self(weak_from_this());
  prune_task_timer_->async_wait(
    [this, weak_self](const asio::error_code& ec)
  {
    if (ec == asio::error::operation_aborted)
      return;

    auto self = weak_self.lock();
    if (!self)
      return;

    // Reap anything dormant for 5 minutes
    auto reap_time = std::chrono::system_clock::now() - std::chrono::minutes(5);

    // Prune task
    {
      std::unique_lock<std::mutex> lock(connection_lock_);

      std::vector<std::string> erase_keys;

      for (auto& entry : clients_)
      {
        if (entry.second.last <= reap_time)
        {
          entry.second.connection->close();
          erase_keys.push_back(entry.first);
        }
      }

      for (auto& key : erase_keys)
        clients_.erase(key);

      // Now recurse ourselves so that we will wait again
      start_background_client_prune_task();
    }
  });
}



