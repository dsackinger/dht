﻿//////////////////////////////////////////////////////////////////////////
// main.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//

#include "Client.h"
#include "Server.h"
#include "Logger.h"

#include <regex>

class ProtobufRAII
{
public:
  ProtobufRAII()
  {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;
  }

  virtual ~ProtobufRAII()
  {
    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
  }
};

struct config_t
{
  typedef enum { node, set, has, get, diag } op_e;

  config_t() : op(node), bindaddress("127.0.0.1"), bindport("11170"), server(), port("11170"), key(), value() {};

  op_e        op;
  std::string bindaddress;
  std::string bindport;
  std::string server;
  std::string port;
  std::string key;
  std::string value;
};

void print_usage()
{
  std::cout
    << "Usage: dht [node] [options] [command] [key] [value]" << std::endl
    << std::endl
    << "  node options:" << std::endl
    << "    --bind=<address> - defaults to 127.0.0.1" << std::endl
    << "    --port=<port> - port to listen on [default = 11170]" << std::endl
    << "    --server=<address>[:port] - to join an existing cache [default port - 11170]" << std::endl
    << std::endl
    << "  client options (node not specified)" << std::endl
    << "    --server=<address[:port]  - an existing node" << std::endl
    << std::endl
    << "  examples:" << std::endl
    << "    Start a node an initial node binding to the default address" << std::endl
    << "      dht node" << std::endl
    << "    Start a node and connect to an existing cache on server:port" << std::endl
    << "      dht node --server=192.168.1.7:13579" << std::endl
    << std::endl
    << "    Set a key/value pair of foo -> bar" << std::endl
    << "      dht --server=192.168.1.7:13579 set foo bar" << std::endl
    << "    Check if a key foo exists" << std::endl
    << "      dht --server=192.168.1.7:13579 check foo" << std::endl
    << "    Get the value associated with key foo" << std::endl
    << "      dht --server=192.168.1.7:13579 get foo" << std::endl
    << "    Dump the caches to the screen for all nodes" << std::endl
    << "      dht --server=192.168.1.7:13579 diag" << std::endl;

  std::exit(1);
}

void parse_args(int argc, char*argv[], config_t& config)
{
  std::cmatch cm;
  std::regex arg_match("--([^=]+)=(.+)");

  for (int i = 1; i < argc; i++)
  {
    std::string arg = argv[i];
    if (arg == "node")
    {
      config.op = config_t::op_e::node;
      continue;
    }

    if (arg == "set")
    {
      config.op = config_t::op_e::set;
      continue;
    }

    if (arg == "has")
    {
      config.op = config_t::op_e::has;
      continue;
    }

    if (arg == "get")
    {
      config.op = config_t::op_e::get;
      continue;
    }

    if (arg == "diag")
    {
      config.op = config_t::op_e::diag;
      continue;
    }

    if (arg == "--help")
    {
      print_usage();
      std::exit(3);
    }

    auto match = std::regex_match(arg.c_str(), cm, arg_match, std::regex_constants::match_default);
    if (match)
    {
      // cm[0] == full match
      // cm[1] == first capture
      // cm[2] == second capture

      // Figure out what parameter was sent
      if (cm[1] == "bind")
      {
        config.bindaddress = cm[2];
        continue;
      }

      if (cm[1] == "port")
      {
        config.bindport = cm[2];
        continue;
      }

      if (cm[1] == "server")
      {
        auto v = cm[2].str();
        auto pos = v.find(':');
        if (pos != v.npos)
        {
          config.server = v.substr(0, pos);
          config.port = v.substr(pos + 1);
        }
        else
        {
          // Default to port 11170
          config.server = v;
          config.port = "11170";
        }
        continue;
      }

      print_usage();
    }
    else
    {
      // Not a match, then first is key and second is value
      if (config.key.empty())
      {
        config.key = argv[i];
        continue;
      }

      // Rest of parameters build the value which must be last
      config.value = argv[i];
      for (int left = i + 1; left < argc; left++)
      {
        config.value += ' ';
        config.value += argv[left];
      }

      // No more arguments - We consumed the rest of them.
      break;
    }
  }
}

int run_node(config_t& config)
{
  Logger log;

  auto server = std::make_shared<Server>(log);
  server->start(config.bindaddress, config.bindport);

  if (!config.server.empty())
  {
    log.log("Connecting to DHT at: ", config.server, ":", config.port);
    server->join(config.server, config.port);
  }

  while (!server->is_exiting())
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}

int run_client(config_t& config)
{
  Logger log;

  // Start establishing the connection
  try
  {
    auto client(std::make_shared<Client>(config.server, config.port, log));
    client->start();

    switch (config.op)
    {
    case config_t::op_e::set:
      if (client->set_value(config.key, config.value))
        log.log("Value [", config.key, "] was set to [", config.value, "]");
      else
        log.log("Failed to set value for key [", config.key, "]");
      break;

    case config_t::op_e::has:
      if (client->has_value(config.key))
        log.log("Cache has value for key [", config.key, "]");
      else
        log.log("Cache does not have a value for key [", config.key, "]");
      break;

    case config_t::op_e::get:
    {
      std::string value;
      client->get_value(config.key, value);

      if (!value.empty())
        log.log("The Value for key [", config.key, "] is [", value, "]");
      else
        log.log("Cache does not have a value for key [", config.key, "]");
    }
    break;

    case config_t::op_e::diag:
      if (client->send_diag())
        log.log("Sent diag to nodes");
      else
        log.log("Failed to send diag to nodes");
      break;

    default:
      log.log("Unknown op: ", static_cast<int>(config.op));
    }
  }
  catch(std::exception e)
  {
    log.log("Exception: ", e.what());
  }

  return 0;
}

int main(int argc, char*argv[])
{
  // Initialize Protobuf
  ProtobufRAII proto;

  config_t config;
  parse_args(argc, argv, config);

  if (config.op == config_t::op_e::node)
    run_node(config);
  else
    run_client(config);

  return 0;
}

