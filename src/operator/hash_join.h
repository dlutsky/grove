#ifndef HASH_JOIN_H
#define HASH_JOIN_H

#include <utility>
#include <vector>
#include <map>
#include <ska/flat_hash_map.hpp>
#include "operator.h"
#include "util/memory_pool.h"
#include "plan/query_plan.h"


class HashJoin : public Operator {
public:
  HashJoin(Operator* left, const std::map<uint32_t, Resource*>& left_resources, Operator* right, const std::map<uint32_t, Resource*>& right_resources, const std::map<uint32_t, Resource*>& output_resources, const std::vector<JoinNode>& join_nodes, double expected_cardinality);
  ~HashJoin();

  void open();
  void close();

  bool first();
  bool next();
  bool seek();

  double estimateCost();

protected:
  struct Entry {
    Entry* next;
    uint32_t value;
    Entry(uint32_t key) : next(nullptr), value(value) {}
  };
  class HashTable {
  public:
    HashTable();

    void insert(uint32_t key, uint32_t value);
    Entry* lookup(uint32_t key);
    bool exist(uint32_t key);

    int numOfKeys();
    bool getAllKeys(std::vector<uint32_t>& keys);

  private:
    MemoryPool<Entry> pool;
    ska::flat_hash_map<uint32_t, Entry*> table;
    int num_keys;
  };

  bool build();
  bool probe();

  Operator *left, *right;
  std::map<uint32_t, Resource*> left_resources, right_resources;
  std::map<uint32_t, Resource*> output_resources;

  std::vector<std::pair<Resource*, Resource*>> left_bind_resources, right_bind_resources;

  std::vector<JoinNode> join_nodes;

  HashTable hash_table;

  int curr_idx;
  Resource* right_key_res;
};


#endif
