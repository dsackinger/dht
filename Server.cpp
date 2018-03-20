//////////////////////////////////////////////////////////////////////////
// Server.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the Server class

#include "Server.h"

#include <functional>

Server::Server(Logger& log)
  : log_(log)
  , exiting_(false)
  , in_join_(false)
  , pool_(std::make_shared<ThreadPool>(1, log))
  , listener_()
  , calls_(std::make_shared<CallManager>(pool_))
{
  // Get our pool up and running
  pool_->start();
  
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

  log_.log("Node ID [", id_, "]");

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
    listener_ = std::make_shared<TcpListener>(pool_->get_io_service(), address, port, shared_from_this(), log_);
  }
  else
  {
    // Pick the default v4 address to bind
    auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), std::atoi(port.c_str()));
    listener_ = std::make_shared<TcpListener>(pool_->get_io_service(), endpoint, shared_from_this(), log_);
  }

  // Now start the listener
  listener_->start();

  // Add ourself into our node address map
  node_addresses_[id_] = listener_->get_name();
}

void Server::stop()
{
  // Shut down our server
  exiting_ = true;
}

// Here we will attempt to join an existing network
void Server::join(std::string& address, std::string& port)
{
  try
  {
    auto connection = std::make_shared<TcpConnection>(pool_->get_io_service(), address, port, log_);
    connection->set_listener(shared_from_this());
    connection->start();

    auto self = shared_from_this();
    auto callid = calls_->register_callback(
      [this, self, connection](const dht::AckMsg& ack)
    {
      if (!ack.success())
      {
        log_.log("Failed to send join request");
        stop();
        return;
      }

      if (ack.has_uint64_value())
      {
        std::uint64_t acceptor_id = ack.uint64_value();

        // The acceptor sent his ID back in the result to let us know success
        log_.log("Our Join Request is being granted by Node [", acceptor_id, "]");

        // Create connections for all of the nodes that we don't know
        {
          std::unique_lock<std::mutex> lock(connection_lock_);
          nodes_[acceptor_id] = connection;
          node_addresses_[acceptor_id] = connection->get_connection_name();
        }
      }
      else
      {
        log_.log("Our join request was rejected");
        connection->close();
        stop();
        return;
      }

      // NOTE: Here is where we might do additional information.
      // The node will send us a message to introduce us to the network.
    });

    CallClient::send_join(connection, callid, id_, listener_->get_name());
  }
  catch (std::exception e)
  {
    log_.log("Exception occurred: ", e.what());
    return;
  }
}


// IConnectionListener interface
void Server::incoming_tcp_connection(std::shared_ptr<TcpConnection> connection)
{
  // We have a new connection.  We don't know anything until it talks to us.  For now, just
  // put him in the client_connections_ structure
  auto ep = connection->get_connection_name();
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
  else if (msg.has_join_msg())
    on_msg_join(connection, msg);
  else if (msg.has_join_accept_msg())
    on_msg_join_accept(connection, msg);
  else if (msg.has_join_node_id_msg())
    on_msg_join_node_id(connection, msg);
  else if (msg.has_join_notify_msg())
    on_msg_join_notify(connection, msg);
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
  auto& ack_msg = msg.ack_msg();
  auto callid = ack_msg.callid();
  auto callback = calls_->get_callback(callid);
  if (!callback)
    log_.log("Received an ack for an unknown callback [", callid, "]");
  else
  {
    callback(msg.ack_msg());
    calls_->remove_callback(callid);
  }
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
  auto callid = calls_->register_callback(
    [this, connection, caller_callid](const dht::AckMsg& ack)
  {
    // We just pass back through to the client
    CallClient::send_ack(connection, caller_callid, true);
  });

  CallClient::send_set(node_connection, callid, key, value, id_);
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
    CallClient::send_ack(connection, caller_callid, true);
    return;
  }

  // If we have an old table, check that one too
  if (old_table_ && old_table_->has(key))
  {
    CallClient::send_ack(connection, caller_callid, true);
    return;
  }

  // See if this call is coming from another node.  If it is coming
  // from another node, we don't allow for forwarding
  if (msg.has_node_id())
  {
    // Not in our cache
    CallClient::send_ack(connection, caller_callid, false);
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
    CallClient::send_ack(connection, caller_callid, answer);
  };

  // Try to ask our target
  auto ret = on_msg_has_ask_remote(key, current_table_,
    [this, connection, caller_callid, key, old_response_callback](const dht::AckMsg& ack)
  {
    if (ack.success() && ack.bool_value())
    {
      // Found it in the target's current table
      CallClient::send_ack(connection, caller_callid, true);
      return;
    }

    // Fall back to trying our old table
    if (on_msg_has_ask_remote(key, old_table_, old_response_callback))
      return;

    // Couldn't send it... Tell the client we failed
    CallClient::send_ack(connection, caller_callid, false);
  });

  // If we successfully sent it, we will have received the callback
  // and processed the second successfully above
  if (ret)
    return;

  // Couldn't send to client on the new table.  Fall back to try
  // one last time on the old_table if we have it
  if (on_msg_has_ask_remote(key, old_table_, old_response_callback))
    return;

  CallClient::send_ack(connection, caller_callid, false);
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
    CallClient::send_ack(connection, caller_callid, value_out);
    return;
  }

  // If we have an old table, check that one too
  if (old_table_ && old_table_->get(key, value_out))
  {
    CallClient::send_ack(connection, caller_callid, value_out);
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
    CallClient::send_ack(connection, caller_callid, ack.string_value());
  };

  // Try to ask our target
  auto ret = on_msg_get_ask_remote(key, current_table_,
    [this, connection, caller_callid, key, old_response_callback](const dht::AckMsg& ack)
  {
    if (ack.success() && ack.has_string_value())
    {
      // Found it in the target's current table
      CallClient::send_ack(connection, caller_callid, ack.string_value());
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

void Server::diag_notify_nodes(std::uint64_t target)
{
  // Have we gone around the ring
  if (target == id_)
    return;

  // Tell the target about the join
  auto callid = calls_->register_callback(
    [this, target](const dht::AckMsg& ack)
  {
    if (!ack.success())
    {
      log_.log("Failed to send diag to Node [", target, "]");
      return;
    }

    if (!ack.bool_value())
    {
      log_.log("Node [", target, "] failed out diag request");
      return;
    }

    // Move on to the next 
    auto next_node = get_next_node_id(target);
    pool_->get_io_service().post([this, next_node]() { diag_notify_nodes(next_node); });
  });

  // Grab the updated list
  log_.log("Sending diag to Node [", target, "]");

  auto node_connection = get_node_connection(target);
  if (!node_connection)
  {
    log_.log("Failing diag - We do not have a connection to Node [", target, "]");
    return;
  }

  CallClient::send_diag(node_connection, callid, id_);
}

void Server::on_msg_diag(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& diag_msg = msg.diag_msg();

  // Send him an ack that we got the message - We will take it from here
  CallClient::send_ack(connection, caller_callid, true);

  log_.log("Processing diag message");
  dump_cache();

  // We don't forward this message
  if (msg.has_node_id())
    return;

  // Forward to all of our other nodes
  auto next = get_next_node_id();
  pool_->get_io_service().post([this, next]() { diag_notify_nodes(next); });
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

void Server::on_msg_join(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& join_msg = msg.join_msg();
  std::uint64_t joinid = join_msg.id();

  // We received a join request from a node who is coming up.
  if (in_join_)
  {
    // Already in a join process.  Tell him no
    log_.log("Already in a join.  Rejecting request from node [", joinid, "]");
    CallClient::send_ack(connection, caller_callid, false);
    return;
  }

  if (current_table_->has_node(joinid))
  {
    log_.log("Already have a node with id [", joinid, "]");
    CallClient::send_ack(connection, caller_callid, false);
    return;
  }

  // Set the join Flag
  auto address = join_msg.address();
  log_.log("Entering join mode for Node [", joinid, "] listening at [", address, "]");
  in_join_ = true;

  // Take a copy of the current list of nodes and send it to the new guy
  node_vector_t nodes(current_table_->nodes());
  nodes.push_back(joinid);

  // Make a new table
  old_table_ = current_table_;
  current_table_ = std::make_shared<HashTable>(0, nodes);

  // Promote the connection
  join_promote_connection_to_node(connection, joinid, address);

  auto next_node = get_next_node_id();

  // Notify other nodes that we have a new node
  pool_->get_io_service().post([this, connection, caller_callid, next_node, joinid]()
  {
    join_notify_nodes(connection, caller_callid, next_node, joinid);
  });
}

void Server::join_notify_nodes(std::shared_ptr<TcpConnection> connection, std::uint32_t caller_callid, std::uint64_t target, std::uint64_t joinid)
{
  if (target == id_)
  {
    // We have notified everyone.  Move on to telling the node he is good
    // Ack his join request and pass our NodeID back in the result
    CallClient::send_ack(connection, caller_callid, id_);

    // Now post a call to give him the information he needs
    pool_->get_io_service().post([this, connection, joinid]() { join_accept_node(connection, joinid); });
    return;
  }

  if (target == joinid)
  {
    // Skip the joinid guy.  He knows he is joining already.
    auto next_node = get_next_node_id(target);
    pool_->get_io_service().post([this, connection, caller_callid, next_node, joinid]()
    {
      join_notify_nodes(connection, caller_callid, next_node, joinid);
    });
    return;
  }

  // Tell the target about the join
  auto callid = calls_->register_callback(
    [this, connection, caller_callid, target, joinid](const dht::AckMsg& ack)
  {
    if (!ack.success())
    {
      log_.log("Failed to contact Node [", target, "] about join for Node [", joinid, "]");
      return;
    }

    if (!ack.bool_value())
    {
      log_.log("Node [", target, "] rejected our join request for Node [", joinid, "]");
      return;
    }

    // Move on to the next 
    auto next_node = get_next_node_id(target);
    pool_->get_io_service().post([this, connection, caller_callid, next_node, joinid]()
    {
      join_notify_nodes(connection, caller_callid, next_node, joinid);
    });
  });

  // Grab the updated list
  log_.log("Notifying Node [", target, "] that node [", joinid, "] is joining");

  auto node_info = get_node_info_list();
  auto node_connection = get_node_connection(target);
  if (!node_connection)
  {
    log_.log("Failing join - We do not have a connection to Node [", target, "]");
    return;
  }

  CallClient::send_join_notify(node_connection, callid, id_, node_info);
}

void Server::join_accept_node(std::shared_ptr<TcpConnection> connection, std::uint64_t joinid)
{
  // Tell the caller he can join our dht
  auto callid = calls_->register_callback(
    [this, connection, joinid](const dht::AckMsg& ack)
  {
    if (!ack.success())
    {
      log_.log("Client [", joinid, "] failed to join?  Can we go back to the other config?");
      return;
    }

    if (!ack.bool_value())
    {
      log_.log("Client [", joinid, "] rejected our join confirmation");
      return;
    }

    // We have completed our join duties at this point.
    log_.log("Node [", connection->get_connection_name(), "] has been promoted to Node [", joinid, "] - Welcome to the DHT.");

    // Trigger our own redistribution to start
    log_.log("Starting table redistribution.");
    pool_->get_io_service().post([this]() { join_distribute_old_table(); });
  });

  // Grab the updated list
  auto node_info = get_node_info_list();
  CallClient::send_join_accept(connection, callid, id_, node_info);
}


void Server::on_msg_join_accept(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& jam = msg.join_accept_msg();

  auto acceptor_id = jam.acceptor_id();

  log_.log("Join has been accepted");

  // Collect the list of nodes to initialize our table
  std::vector<uint64_t> nodes;
  for (int i = 0; i < jam.nodes_size(); i++)
  {
    auto& node = jam.nodes(i);
    std::uint64_t nodeid = node.id();
    nodes.push_back(nodeid);
  }

  // Dump our previous state and setup to be part of the dht
  old_table_.reset();
  current_table_ = std::make_shared<HashTable>(0, nodes);

  // Let him know we are up and going
  CallClient::send_ack(connection, caller_callid, true);

  // Now loop to see if we need to establish any more connections
  for (int i = 0; i < jam.nodes_size(); i++)
  {
    auto& node = jam.nodes(i);
    std::uint64_t nodeid = node.id();
    std::string address = node.address();
    std::string port = "11170";

    // Check to see if the connection is either for us or the acceptor
    if (id_ == nodeid || acceptor_id == nodeid)
      continue;

    // See if a port was specified    
    auto colon = address.find(':');
    if (address.npos != colon)
    {
      port = address.substr(colon + 1);
      address = address.substr(0, colon);
    }

    try
    {
      log_.log("Establish connection to Node [", nodeid, "] at [", address, ":", port, "]");
      auto node_connection = std::make_shared<TcpConnection>(pool_->get_io_service(), address, port, log_);
      node_connection->set_listener(shared_from_this());
      node_connection->start();

      // Dump him in our connection map
      {
        std::unique_lock<std::mutex> lock(connection_lock_);
        nodes_[nodeid] = node_connection;
        node_addresses_[nodeid] = node_connection->get_connection_name();
      }

      // Now send him our info
      auto callid = calls_->register_callback(
        [this, node_connection](const dht::AckMsg& ack)
      {
        if (!ack.success())
          log_.log("Failed to send NodeInfo message to [", node_connection->get_connection_name(), "]");
        else
        {
          if (!ack.bool_value())
            log_.log("Node at [", node_connection->get_connection_name(), "] will not acknowledge us as a node");
          else
            log_.log("Node at [", node_connection->get_connection_name(), "] acknowledged us as a node");
        }
      });

      CallClient::send_join_node_id(node_connection, callid, { id_, listener_->get_name() });
    }
    catch (std::exception e)
    {
      log_.log("Exception connection to node at [", node.address(), "]: ", e.what());
      std::exit(4);
    }
  }

  // Now make sure we come out of join mode ourselves
  log_.log("Leaving client Join Mode");
  in_join_ = false;
}

void Server::on_msg_join_node_id(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& jnid = msg.join_node_id_msg();
  std::uint64_t newid = jnid.id();
  std::string address = jnid.address();

  // If we aren't in a join state, we won't accept a new node
  if (!in_join_)
  {
    log_.log("Rejecting node [", newid, "] at [", connection->get_connection_name(), "] because we are not in a join state");
    CallClient::send_ack(connection, caller_callid, false);
    return;
  }

  // See if this is a node we are supposed to know
  if (!current_table_->has_node(newid))
  {
    log_.log("Rejecting node [", newid, "] at [", connection->get_connection_name(), "] because he is not on the allowed list");
    CallClient::send_ack(connection, caller_callid, false);
    return;
  }

  // Promote him and let him know we acknowledge him
  log_.log("Promoting connection [", connection->get_connection_name(), " to Node [", newid, "] listening at [", address, "]");
  join_promote_connection_to_node(connection, newid, address);
  CallClient::send_ack(connection, caller_callid, true);

  // Now we are fully connected again... Leave join state
  log_.log("Leaving Join Mode");
  in_join_ = false;

  log_.log("Starting redistribution");
  pool_->get_io_service().post([this]() { join_distribute_old_table(); });
}

void Server::on_msg_join_notify(std::shared_ptr<TcpConnection> connection, const dht::Msg& msg)
{
  auto caller_callid = msg.id();
  auto& jn = msg.join_notify_msg();
  auto acceptor_id = jn.acceptor_id();

  log_.log("Received a join notification request from Node [", acceptor_id, "]");

  if (in_join_)
  {
    log_.log("Rejecting join request because we are already in a join state");
    CallClient::send_ack(connection, caller_callid, false);
    return;
  }

  // We received a join notify message.  Stick us in the new state
  log_.log("Entering Join state waiting for Node to connect");
  in_join_ = true;

  // Collect the list of nodes to initialize our table
  std::vector<uint64_t> nodes;

  // Now loop to see if we need to establish any more connections
  for (int i = 0; i < jn.nodes_size(); i++)
  {
    std::uint64_t nodeid = jn.nodes(i).id();
    nodes.push_back(nodeid);
  }

  // Move our tables
  old_table_ = current_table_;
  current_table_ = std::make_shared<HashTable>(0, nodes);

  CallClient::send_ack(connection, caller_callid, true);
}

// Join Related Functions
void Server::join_promote_connection_to_node(std::shared_ptr<TcpConnection> connection, std::uint64_t joinid, std::string address)
{
  std::unique_lock<std::mutex> lock(connection_lock_);

  // Insert the connection into our connections map
  nodes_[joinid] = connection;
  node_addresses_[joinid] = address;
  clients_.erase(connection->get_connection_name());
}

void Server::join_distribute_old_table()
{
  auto key = old_table_->get_first_key();
  if (key.empty())
  {
    // Migration is done.
    log_.log("Redistribution is complete");
    old_table_.reset();

    // If we were in join mode, we can safely exit that mode now.
    log_.log("Leaving Join Mode.");
    in_join_ = false;

    return;
  }

  // Process the key
  std::string value_out;
  if (!old_table_->get(key, value_out))
  {
    // Did the key move on us?
    old_table_->erase(key);
    pool_->get_io_service().post([this]() { join_distribute_old_table(); });
    return;
  }

  // For readability
  auto& value = value_out;

  auto target = current_table_->node_from_key(key);
  if (target == id_)
  {
    current_table_->set(key, value);
    old_table_->erase(key);
    pool_->get_io_service().post([this]() { join_distribute_old_table(); });
    return;
  }

  // Make sure we have a connection for him
  auto connection = get_node_connection(target);
  if (!connection)
  {
    log_.log("No connection for target [", target, "] dropping key [", key, "]");
    old_table_->erase(key);
    pool_->get_io_service().post([this]() { join_distribute_old_table(); });
    return;
  }

  // OK.. Need to make a call at this point
  auto callid = calls_->register_callback(
    [this, key](const dht::AckMsg& ack)
  {
    if (!ack.success())
      log_.log("Failed to migrate key [", key, "] dropping key");
    else if (!ack.bool_value())
      log_.log("Target rejected key [", key, "] dropping key");

    old_table_->erase(key);
    pool_->get_io_service().post([this]() { join_distribute_old_table(); });
    return;
  });

  log_.log("Sending key [", key, "] to Node [", target, "]");
  CallClient::send_set(connection, callid, key, value, id_);
}


// Utility functions
uint64_t Server::get_next_node_id(uint64_t id)
{
  if (id == 0)
    return current_table_->next_node_id(id_);
  else
    return current_table_->next_node_id(id);
}

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

std::shared_ptr<TcpConnection> Server::get_client_connection(std::string name)
{
  // Working with connection requires the connection lock
  std::unique_lock<std::mutex> lock(connection_lock_);

  auto entry = clients_.find(name);
  if (entry == clients_.end())
    return std::shared_ptr<TcpConnection>();

  return entry->second.connection;
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

std::vector<CallClient::node_info_t> Server::get_node_info_list()
{
  // Grab the updated list
  auto nodes = current_table_->nodes();
  std::vector<CallClient::node_info_t> node_info;
  { // Scope for lock
    std::unique_lock<std::mutex> lock(connection_lock_);
    for (auto& node_id : nodes)
    {
      auto entry = node_addresses_.find(node_id);
      if (entry == node_addresses_.end())
      {
        log_.log("Failed to find an address for Node [", node_id, "]");
        continue;
      }

      auto& address = entry->second;
      node_info.push_back({ node_id, address });
    }
  }

  return node_info;
}


void Server::start_background_client_prune_task()
{

  prune_task_timer_ = std::make_shared<timer_t>(pool_->get_io_service(), std::chrono::seconds(5));

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



