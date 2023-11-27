#include "leapfrog_trie_join.h"
#include <iostream>

LeapFrogTrieJoin::LeapFrogTrieJoin(const std::vector<Operator*>& inputs, const std::vector<std::map<uint32_t, Resource*>>& input_resources, const std::map<uint32_t, Resource*>& output_resources, const std::vector<JoinNode>& join_nodes, double expected_cardinality)
  : Operator(expected_cardinality), inputs(inputs), input_resources(input_resources), output_resources(output_resources), bind_resources(input_resources.size()), join_nodes(join_nodes), trie_iterators(join_nodes.size()) {}

LeapFrogTrieJoin::~LeapFrogTrieJoin() {}

void LeapFrogTrieJoin::open() {
  for(int i=0; i<inputs.size(); i++) {
    inputs[i]->open();
  }

  /*
  for(int i=0; i<join_nodes.size(); ++i) {
    for(int j=0; j<join_nodes[i].join_keys.size(); ++j) {
      std::cout<<join_nodes[i].join_keys[j]<<std::endl;
    }
  }
  */

  std::map<uint32_t, Resource*> join_keys;
  for(int i=join_nodes.size()-1; i>=0; --i) {
    for(int j=0; j<join_nodes[i].join_keys.size(); ++j) {
      uint32_t key = join_nodes[i].join_keys[j];
      if(join_keys.count(key)) {
        trie_iterators[i].join_resources.push_back(std::make_pair(input_resources[i][key], join_keys[key]));
      }
      join_keys[key] = input_resources[i][key];
    }
  }
  join_keys.clear();
  for(int i=0; i<join_nodes.size(); ++i) {
    for(int j=0; j<join_nodes[i].join_keys.size(); ++j) {
      uint32_t key = join_nodes[i].join_keys[j];
      if(join_keys.count(key)) {
        trie_iterators[i].recall_resources.push_back(std::make_pair(input_resources[i][key], join_keys[key]));
      } else {
        join_keys[key] = input_resources[i][key];
      }
    }
  }

  std::set<uint32_t> checked;
  uint32_t join_res = UINT32_MAX;
  for(int i=input_resources.size()-1; i>=0; i--) {
    for(std::map<uint32_t, Resource*>::iterator iter = input_resources[i].begin(), end = input_resources[i].end(); iter != end; ++iter) {
      if(checked.count(iter->first) != 0) {
        continue;
      }

      if(output_resources.count(iter->first) != 0) {
        bind_resources[i].push_back(std::make_pair(iter->second, output_resources[iter->first]));
        checked.insert(iter->first);
      }
    }
  }
}

void LeapFrogTrieJoin::close() {
  for(int i=0; i<inputs.size(); i++) {
    this->inputs[i]->close();
  }
}


bool LeapFrogTrieJoin::first() {
  for(int i=0; i<inputs.size(); i++) {
    this->inputs[i]->first();
  }

  bool found = false;
  int size = 0;
  do {
    int count = input_resources[0].begin()->second->column.size();
    while(trie_iterators[0].idx < count) {
      for(int j=0; j<trie_iterators[0].join_resources.size(); ++j) {
        trie_iterators[0].join_resources[j].second->id = trie_iterators[0].join_resources[j].first->column[trie_iterators[0].idx];
        //std::cout<<trie_iterators[0].join_resources[j].second->id<<std::endl;
      }
      size = enumerate(1);
      if(size != 0) {
        found = true;
      }
      ++trie_iterators[0].idx;
    }
  } while(this->inputs[0]->next());

  return found;
}

int LeapFrogTrieJoin::enumerate(int i) {
  int size = 0;
  if(this->inputs[i]->seek()) {
    //std::cout<<"seek "<<i<<std::endl;
    while(this->inputs[i]->next()) {}
    int count = input_resources[i].begin()->second->column.size();
    if(i == this->inputs.size()-1) {
      //std::cout<<"found"<<std::endl;
      while(trie_iterators[i].idx < count) {
        for(int j=0; j<bind_resources.size(); ++j) {
          for(int k=0; k<bind_resources[j].size(); ++k) {
            bind_resources[j][k].second->column.push_back(bind_resources[j][k].first->column[trie_iterators[j].idx]);
          }
        }
        ++size;
        ++trie_iterators[i].idx;
      }
    } else {
      int s = 0;
      while(trie_iterators[i].idx < count) {
        for(int j=0; j<trie_iterators[i].join_resources.size(); ++j) {
          trie_iterators[i].join_resources[j].second->id = trie_iterators[i].join_resources[j].first->column[trie_iterators[i].idx];
        }
        s = enumerate(i+1);
        size += s;
        /*
        if(s != 0) {
          int m = 0;
          while(true) {
            bool equal = true;
            for(int j=0; j<trie_iterators[i].join_resources.size(); ++j) {
              if(trie_iterators[i].join_resources[j].first->column[trie_iterators[i].idx+1] != trie_iterators[i].join_resources[j].first->column[trie_iterators[i].idx]) {
                equal = false;
                break;
              }
            }
            if(!equal) {
              break;
            }
            ++m;
            ++trie_iterators[i].idx;
          }
          if(m != 0) {
            int n = m*s;
            for(int j=0; j<bind_resources.size(); ++j) {
              if(j==i) {
                for(int k=0; k<bind_resources[j].size(); ++k) {
                  bind_resources[j][k].second->column.insert(bind_resources[j][k].second->column.end(), n, bind_resources[j][k].first->column[trie_iterators[j].idx]);
                }
              } else {
                for(int k=0; k<bind_resources[j].size(); ++k) {
                  bind_resources[j][k].second->column.resize(bind_resources[j][k].second->column.size() + n);
                  for(int t=1; t<=m; ++t) {
                    std::copy(bind_resources[j][k].second->column.end()-n-s, bind_resources[j][k].second->column.end()-n, bind_resources[j][k].second->column.end()-t*s);
                  }
                }
              }
            }
            size += m;
          }
        } else {
          while(true) {
            bool equal = true;
            for(int j=0; j<trie_iterators[i].join_resources.size(); ++j) {
              if(trie_iterators[i].join_resources[j].first->column[trie_iterators[i].idx+1] != trie_iterators[i].join_resources[j].first->column[trie_iterators[i].idx]) {
                equal = false;
                break;
              }
            }
            if(!equal) {
              break;
            }
            ++trie_iterators[i].idx;
          }
        }
        */
        ++trie_iterators[i].idx;
      }
    }
  }

  for(std::map<uint32_t, Resource*>::iterator iter = input_resources[i].begin(), end = input_resources[i].end(); iter != end; ++iter) {
    iter->second->column.clear();
  }
  trie_iterators[i].idx = 0;
  
  return size;
}

bool LeapFrogTrieJoin::next() {
  return false;
}

bool LeapFrogTrieJoin::seek() {
  return false;
}

double LeapFrogTrieJoin::estimateCost() {
  return expected_cardinality;
}