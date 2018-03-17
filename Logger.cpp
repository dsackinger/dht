//////////////////////////////////////////////////////////////////////////
// Logger.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the logger class
//

#include "Logger.h"

#include <ctime>
#include <iostream>

void Logger::log(const std::string& message)
{
  std::unique_lock<std::mutex> scope_lock_(lock_);

  auto t(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  std::string time_string = std::ctime(&t);

  time_string[time_string.length() - 1] = '\0';
  std::cout << time_string << " | " << message.c_str() << std::endl;
}

