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

uint64_t HashTable::node_from_key(const std::string& key)
{
  // Lets hope this doesn't ever happen
  if (nodes_.empty())
    return 0;

  // First, check to see if we should have this key
  auto hasher = table_.hash_function();
  auto hash = hasher(key);

  auto slot = hash % nodes_.size();
  return nodes_.at(slot);
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

void HashTable::del(const std::string& key)
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
