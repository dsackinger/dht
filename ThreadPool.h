//////////////////////////////////////////////////////////////////////////
// ThreadPool.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// ThreadPool class:
//  Creates the requested number of threads to service an io_service
//  The io_service is available for external use
//

#if !defined(__THREAD_POOL_H__)
#define __THREAD_POOL_H__

#include "Logger.h"

#include <asio.hpp>

#include <thread>
#include <vector>

class ThreadPool
{
public:
  ThreadPool(std::size_t threads, Logger& log);
  virtual ~ThreadPool();

public:
  inline asio::io_service& get_io_service() { return io_service_; };

private:
  asio::io_service io_service_;
  asio::io_service::work not_real_work_;
  std::vector<std::thread> threads_;
  Logger& log_;
};

#endif // #if !defined(__THREAD_POOL_H__)
