//////////////////////////////////////////////////////////////////////////
// HashTable.cpp
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// Implementation of the HashTable class

#include "HashTable.h"

#include <algorithm>

HashTable::HashTable(int id, node_vector_t& nodes)
  : id_(id)
  , nodes_(nodes)
  , table_()
  , table_lock_()
{
  // The vector needs to be sorted to enable the mod to work correctly.
  std::sort(nodes_.begin(), nodes_.end());
}

HashTable::~HashTable()
{
}

std::uint64_t HashTable::node_from_key(const std::string& key)
{
  // Lets hope this doesn't ever happen
  if (nodes_.empty())
    return 0;

  auto hash = hash_string(key);
  auto slot = hash % nodes_.size();
  return nodes_.at(slot);
}

std::uint64_t HashTable::next_node_id(std::uint64_t id)
{
  for (std::size_t i = 0; i < nodes_.size(); i++)
  {
    if (nodes_[i] == id)
    {
      i = (i + 1) % nodes_.size();
      return nodes_[i];
    }
  }

  // What?  We didn't find our own id?  Maybe abort due to invalid configuration
  return 0;
}

bool HashTable::has_node(uint64_t id)
{
  std::unique_lock<std::mutex> lock(table_lock_);

  for (std::size_t i = 0; i < nodes_.size(); i++)
    if (nodes_[i] == id)
      return true;

  return false;
}

std::size_t HashTable::node_count()
{
  std::unique_lock<std::mutex> lock(table_lock_);
  return nodes_.size();
}

node_vector_t HashTable::nodes()
{
  std::unique_lock<std::mutex> lock(table_lock_);
  return node_vector_t(nodes_);
}

std::size_t HashTable::get_key_count()
{
  std::unique_lock<std::mutex> lock(table_lock_);
  return table_.size();
}

std::string HashTable::get_first_key()
{
  std::unique_lock<std::mutex> lock(table_lock_);
  if (table_.empty())
    return std::string();
  else
    return table_.begin()->first;
}

void HashTable::set(const std::string& key, const std::string& value)
{
  std::unique_lock<std::mutex> lock(table_lock_);
  table_[key] = value;
}

bool HashTable::has(const std::string& key)
{
  // First, check to see if we should have this key
  std::unique_lock<std::mutex> lock(table_lock_);
  auto entry = table_.find(key);
  return !(entry == table_.end());
}

bool HashTable::get(const std::string& key, std::string& value_out)
{
  std::unique_lock<std::mutex> lock(table_lock_);
  auto entry = table_.find(key);
  if (entry == table_.end())
    return false;

  value_out = entry->second;
  return true;
}

void HashTable::erase(const std::string& key)
{
  std::unique_lock<std::mutex> lock(table_lock_);
  auto entry = table_.find(key);

  if (entry != table_.end())
    table_.erase(entry);
}

std::vector<std::string> HashTable::keys()
{
  std::unique_lock<std::mutex> lock(table_lock_);

  std::vector<std::string> keys;
  for (auto& entry : table_)
    keys.push_back(entry.first);

  return keys;
}

// We need a consistent hash for our values that will work across platforms.
// Simple implementation borrowed and adapted from:
// https://stackoverflow.com/questions/8317508/hash-function-for-a-string
static constexpr std::uint64_t first_sc(37);
static constexpr std::uint64_t a_sc(54059);
static constexpr std::uint64_t b_sc(76963);
std::uint64_t HashTable::hash_string(const std::string& value)
{
  const char *s = value.c_str();
  std::uint64_t h = first_sc;
  while (*s)
  {
    h = (h * a_sc) ^ (static_cast<std::uint64_t>(*s) * b_sc);
    s++;
  }

  return h; // or return h % C;
}

