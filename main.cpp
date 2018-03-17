//////////////////////////////////////////////////////////////////////////
// main.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//

#include "ThreadPool.h"
#include "TcpListener.h"
#include "Logger.h"

class TestListener : public IConnectionListener
{
public:
  TestListener(Logger& log) : log_(log) {};
  virtual ~TestListener() = default;

public:
  // IConnectionListener interface
  void incoming_tcp_connection(std::shared_ptr<TcpConnection> connection) override
  {
    log_.log("Incoming connection!");
  };

private:
  Logger& log_;
};

int main()
{
  Logger log;
  {
    ThreadPool pool(1, log);

    auto server = std::make_shared<TestListener>(log);
    auto listener = std::make_shared<TcpListener>(pool.get_io_service(), "localhost", "9968", server, log);

    bool exit = false;
    while (!exit)
      std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}

