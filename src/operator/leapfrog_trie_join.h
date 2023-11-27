#ifndef LEAPFROG_TRIE_JOIN_H
#define LEAPFROG_TRIE_JOIN_H

#include <utility>
#include <vector>
#include <map>
#include "operator.h"
#include "plan/query_plan.h"

class LeapFrogTrieJoin : public Operator {
public:
  LeapFrogTrieJoin(const std::vector<Operator*>& inputs, const std::vector<std::map<uint32_t, Resource*>>& input_resources, const std::map<uint32_t, Resource*>& output_resources, const std::vector<JoinNode>& join_nodes, double expected_cardinality);
  ~LeapFrogTrieJoin();

  void open();
  void close();

  bool first();
  bool next();
  bool seek();

  double estimateCost();

protected:
  struct TrieIterator {
    std::vector<std::pair<Resource*, Resource*>> join_resources;
    std::vector<std::pair<Resource*, Resource*>> recall_resources;
    int idx;
    TrieIterator() : idx(0) {}
  };

  int enumerate(int i);

  std::vector<Operator*> inputs;
  std::vector<std::map<uint32_t, Resource*>> input_resources;
  std::map<uint32_t, Resource*> output_resources;

  std::vector<std::vector<std::pair<Resource*, Resource*>>> bind_resources;

  std::vector<JoinNode> join_nodes;
  std::vector<TrieIterator> trie_iterators;
};


#endif