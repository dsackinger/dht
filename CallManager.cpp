//////////////////////////////////////////////////////////////////////////
// CallManager.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the CallManager class
//

#include "CallManager.h"

// Some controls
static constexpr auto expire_age_sc(std::chrono::minutes(1));
static constexpr auto reap_age_sc(std::chrono::minutes(5));

CallManager::CallManager(std::shared_ptr<ThreadPool> pool)
  : pool_(pool)
  , next_call_(0)
{
}

CallManager::~CallManager()
{
  if (prune_task_timer_)
    prune_task_timer_->cancel();
}

std::uint32_t CallManager::next_call()
{
  return next_call_++;
}

void CallManager::register_callback(uint32_t id, callback_t callback)
{
  if (!prune_task_timer_)
    start_background_call_prune_task();

  std::unique_lock<std::mutex> lock(call_lock_);
  calls_[id] = { pending, callback, std::chrono::system_clock::now() };
}

std::uint32_t CallManager::register_callback(callback_t callback)
{
  if (!prune_task_timer_)
    start_background_call_prune_task();

  auto thisid = next_call_++;

  std::unique_lock<std::mutex> lock(call_lock_);
  calls_[thisid] = {pending, callback, std::chrono::system_clock::now() };

  return thisid;
}

void CallManager::remove_callback(std::uint32_t id)
{
  std::unique_lock<std::mutex> lock(call_lock_);
  calls_.erase(id);
}

CallManager::call_state_e CallManager::get_state(std::uint32_t id)
{
  std::unique_lock<std::mutex> lock(call_lock_);
  auto call = calls_.find(id);
  if (call == calls_.end())
    return call_state_e::invalid;

  // Found it!
  return call->second.state;
}

CallManager::callback_t CallManager::get_callback(std::uint32_t id)
{
  std::unique_lock<std::mutex> lock(call_lock_);
  auto call = calls_.find(id);
  if (call == calls_.end())
    return callback_t();

  // Found it!
  return call->second.callback;
}


void CallManager::start_background_call_prune_task()
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

    auto now = std::chrono::system_clock::now();
    auto expire_time = now - expire_age_sc;
    auto reap_time = std::chrono::system_clock::now() - reap_age_sc;

    std::vector<call_map_t::key_type> erase_keys;

    // Prune task
    {
      std::unique_lock<std::mutex> lock(call_lock_);

      for (auto& entry : calls_)
      {
        auto id = entry.first;
        auto& callinfo = entry.second;

        // See if it is time to dump the call
        if (callinfo.timestamp <= reap_time)
        {
          // Dumping this call
          erase_keys.push_back(id);
          continue;
        }

        // Check to see if it timed out
        if (callinfo.timestamp <= expire_time && callinfo.state == pending)
          callinfo.state = call_state_e::timeout;
      }
    }

    for (auto& key : erase_keys)
      calls_.erase(key);

      // Now recurse ourselves so that we will wait again
    start_background_call_prune_task();
  });
}

