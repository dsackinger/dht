//////////////////////////////////////////////////////////////////////////
// ThreadPool.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// ThreadPool implementation
//

#include "ThreadPool.h"

ThreadPool::ThreadPool(std::size_t thread_count, Logger& log)
  : thread_count_(thread_count)
  , io_service_()
  , not_real_work_(io_service_)
  , log_(log)
{
}

ThreadPool::~ThreadPool()
{
  if (!io_service_.stopped())
    io_service_.stop();

  for (auto& thread : threads_)
    thread.join();

  threads_.clear();
}

void ThreadPool::start()
{
  for (auto i = 0; i < thread_count_; i++)
    threads_.emplace_back([this, i]()
  {
    std::string thread_name = std::string("Thread-") + std::to_string(i + 1);
    log_.log("Starting Thread: ", thread_name);

    size_t count = io_service_.run();

    log_.log("Processed [", count, "] message on thread ", thread_name);
  });
}

void ThreadPool::shutdown()
{
  if (!io_service_.stopped())
    io_service_.stop();
}