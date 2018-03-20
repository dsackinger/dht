//////////////////////////////////////////////////////////////////////////
// HashTable.h
//
// Copyright (C) 2018 Dan Sackinger - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the MIT license.
//
// HashTable class:
//  This class implements a hash table.
//
//  This is basically a wrapper around a unordered_map with
//  some properties for use with the dht
//

#if !defined(__HASHTABLE_H__)
#define __HASHTABLE_H__

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

typedef std::vector<std::uint64_t> node_vector_t;

class HashTable
{
public:
  HashTable(int id, node_vector_t& nodes);
  ~HashTable();

public:
  std::uint64_t node_from_key(const std::string& key);
  std::uint64_t next_node_id(std::uint64_t id);
  bool has_node(uint64_t id);
  std::size_t node_count();
  node_vector_t nodes();

  std::size_t get_key_count();
  std::string get_first_key();

  void set(const std::string& key, const std::string& value);
  bool has(const std::string& key);
  bool get(const std::string& key, std::string& value_out);
  void erase(const std::string& key);

public:
  std::vector<std::string> keys();

private:
  std::uint64_t hash_string(const std::string& value);

private:
  int id_;
  node_vector_t nodes_;
  std::unordered_map<std::string, std::string> table_;
  std::mutex table_lock_;
};

#endif // #if !defined(__HASHTABLE_H__)

