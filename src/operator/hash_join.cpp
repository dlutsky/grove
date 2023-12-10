#include "hash_join.h"

HashJoin::HashJoin(Operator* left, const std::map<uint32_t, Resource*>& left_resources, Operator* right, const std::map<uint32_t, Resource*>& right_resources, const std::map<uint32_t, Resource*>& output_resources, const std::vector<JoinNode>& join_nodes, double expected_cardinality)
  : Operator(expected_cardinality), left(left), left_resources(left_resources), right(right), right_resources(right_resources), output_resources(output_resources), join_nodes(join_nodes), curr_idx(0) {
}

HashJoin::~HashJoin() {}

void HashJoin::open() {
  left->open();
  right->open();

  std::set<uint32_t> checked;
  for(std::map<uint32_t, Resource*>::iterator iter = right_resources.begin(), end = right_resources.end(); iter != end; ++iter) {
    if(checked.count(iter->first) != 0) {
      continue;
    }

    if(output_resources.count(iter->first) != 0) {
      right_bind_resources.push_back(std::make_pair(iter->second, output_resources[iter->first]));
      checked.insert(iter->first);
    }
  }
  for(std::map<uint32_t, Resource*>::iterator iter = left_resources.begin(), end = left_resources.end(); iter != end; ++iter) {
    if(checked.count(iter->first) != 0) {
      continue;
    }

    if(output_resources.count(iter->first) != 0) {
      left_bind_resources.push_back(std::make_pair(iter->second, output_resources[iter->first]));
      checked.insert(iter->first);
    }
  }
  /*
  HashTableBuilder builder(*this);
  Thread thread(&builder);
  thread->start();
  */
}

void HashJoin::close() {
  left->close();
  right->close();
}


bool HashJoin::first() {
  // build
  build();

  // probe
  return probe();
}

bool HashJoin::next() {
  return probe();
}

bool HashJoin::seek() {
  return false;
}

double HashJoin::estimateCost() {
  return expected_cardinality;
}

bool HashJoin::build() {
  for(std::map<uint32_t, Resource*>::iterator iter = output_resources.begin(); iter != output_resources.end(); ++iter) {
    if(!iter->second->column.empty()) {
      for(std::map<uint32_t, Resource*>::iterator it = left_resources.begin(); it != left_resources.end(); ++it) {
        if(iter->first == it->first) {
          it->second->column = std::move(iter->second->column);
          goto finish;
        }
      }
      for(std::map<uint32_t, Resource*>::iterator it = right_resources.begin(); it != right_resources.end(); ++it) {
        if(iter->first == it->first) {
          it->second->column = std::move(iter->second->column);
          goto finish;
        }
      }
    }
    finish:;
  }

  const JoinNode& left_node = this->join_nodes[0];
  Resource* left_key_res = left_resources[left_node.right_join_key];
  int i = 0;
  if(left->first()) {
    for(; i<left_key_res->column.size(); i++) {
      this->hash_table.insert(left_key_res->column[i], i);
    }
    while(left->next()) {
      for(; i<left_key_res->column.size(); i++) {
        this->hash_table.insert(left_key_res->column[i], i);
      }
    }
  }

  const JoinNode& right_node = this->join_nodes[1];
  right_key_res = right_resources[right_node.left_join_key];
  this->hash_table.getAllKeys(right_key_res->column);
  if(right->first()) {
    while(right->next()) {}
  }
  return !right_key_res->column.empty();
}

bool HashJoin::probe() {
  if(curr_idx >= right_key_res->column.size()) {
    return false;
  }
  Entry* entry = nullptr;
  while(curr_idx < right_key_res->column.size()) {
    entry = this->hash_table.lookup(right_key_res->column[curr_idx]);
    int k = 0;
    while(entry != nullptr) {
      for(int j=0; j<left_bind_resources.size(); j++) {
        left_bind_resources[j].second->column.push_back(left_bind_resources[j].first->column[entry->value]);
      }
      ++k;
      entry = entry->next;
    }

    if(k != 0) {
      for(int j=0; j<right_bind_resources.size(); j++) {
        right_bind_resources[j].second->column.insert(right_bind_resources[j].second->column.end(), k, right_bind_resources[j].first->column[curr_idx]);
      }
    }
    ++curr_idx;
    if(curr_idx % 50000 == 0) {
      return true;
    }
  }
  return true;
}



HashJoin::HashTable::HashTable() {
  this->num_keys = 0;
}

void HashJoin::HashTable::insert(uint32_t key, uint32_t value) {
  Entry* entry = pool.alloc();
  entry->value = value;
  entry->next = nullptr;

  Entry* prev = lookup(key);
  if(prev) {
    entry->next = prev->next;
    prev->next = entry;
  } else {
    ++num_keys;
    table[key] = entry;
  }
}

HashJoin::Entry* HashJoin::HashTable::lookup(uint32_t key) {
  Entry* entry;
  auto it = this->table.find(key);
  if(it != this->table.end()) {
    return it->second;
  }
  return nullptr;
}

bool HashJoin::HashTable::exist(uint32_t key) {
  auto it = this->table.find(key);
  return it != this->table.end();
}

int HashJoin::HashTable::numOfKeys() {
  return num_keys;
}

bool HashJoin::HashTable::getAllKeys(std::vector<uint32_t>& keys) {
  for(ska::flat_hash_map<uint32_t, Entry*>::iterator it = this->table.begin(), end = this->table.end(); it != end; ++it) {
    keys.push_back(it->first);
  }
}

