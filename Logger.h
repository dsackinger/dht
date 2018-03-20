//////////////////////////////////////////////////////////////////////////
// Logger.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Logger declaration:
//  This is a simple logger that unpacks arguments and
//  dumps them to cout
//

#if !defined(__LOGGER_H__)
#define __LOGGER_H__

#include <mutex>
#include <sstream>

class Logger
{
public:
  Logger() = default;
  virtual ~Logger() = default;

public:
  // Public logging function that takes any arguments as long as they output to a stream
  template<typename... Args>
  void log(Args... args)
  {
    std::stringstream ss;
    log(ss, args...);
    log(ss.str());
  }

private:
  // Final type for ending the recursion
  template<typename T>
  void log(std::stringstream& os, T final)
  {
    os << final;
  }

  // Expands to account for all types that can be output to a stream
  template<typename T, typename... Args>
  void log(std::stringstream& os, T current, Args... next)
  {
    os << current;
    log(os, next...);
  }

  // Logs the final message
  void log(const std::string& message);

private:
  std::mutex lock_;

private:
  // Disable assign / copy
  Logger(const Logger& rhs) = delete;
  Logger& operator =(const Logger& rhs) = delete;
};

#endif // #if !defined(__LOGGER_H__)

