//////////////////////////////////////////////////////////////////////////
// CallManager.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// CallManager class:
//  This class coordinates call ids and posts callbacks
//  when messages come back that are being waited for
//

#if !defined(__CALL_MANAGER_H__)
#define __CALL_MANAGER_H__

#include "protobuf/dht.pb.h"

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

class CallManager
  : std::enable_shared_from_this<CallManager>
{
public:
  CallManager(asio::io_service& io_service);
  virtual ~CallManager();

  typedef enum { invalid, pending, completed, timeout } call_state_e;
  typedef std::function<void(const dht::AckMsg& ack)> callback_t;

  std::uint32_t next_call();
  void register_callback(uint32_t id, callback_t callback);
  std::uint32_t register_callback(callback_t callback);
  callback_t get_callback(std::uint32_t id);
  call_state_e get_state(std::uint32_t id);
  void remove_callback(std::uint32_t id);

private:
  // Utility Functions
  void start_background_call_prune_task();

private:
  // Ordinal value for our calls
  std::atomic<std::uint32_t> next_call_;

  // Structure for holding the callbacks
  typedef struct
  {
    call_state_e state;
    callback_t callback;
    std::chrono::system_clock::time_point timestamp;
  } call_entry_t;

  std::mutex call_lock_;
  typedef std::unordered_map<std::uint32_t, call_entry_t> call_map_t;
  call_map_t calls_;

  // Needed for running our cleanup periodically
  asio::io_service& io_service_;

  // Background thread
  typedef asio::basic_waitable_timer<std::chrono::system_clock> timer_t;
  std::shared_ptr<timer_t> prune_task_timer_;
};

#endif // #if !defined(__CALL_MANAGER_H__)
