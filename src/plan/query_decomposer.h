#ifndef QUERY_DECOMPOSER_H
#define QUERY_DECOMPOSER_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <tuple>
#include "query/query_graph.h"
#include "util/bit_set.h"

struct GHDNode {
  enum Type { Single, Star, Cyclic };
  Type type;
  std::vector<int> query_nodes;
  uint32_t common_key;
};

class QueryDecomposer {
public:
  QueryDecomposer();
  ~QueryDecomposer();

  void decompose(const QueryGraph& graph, std::vector<GHDNode>& nodes);

private:
  void splitGraph(const QueryGraph& graph, const std::vector<int>& edges, std::vector<GHDNode>& nodes);
  void findAllVariables(const std::unordered_map<uint32_t, std::vector<std::tuple<uint32_t, int, int>>>& vertices, uint32_t start, uint32_t except, BitSet& variables, std::unordered_set<int>& edges);

  std::unordered_map<uint32_t, std::vector<int>> res_to_node;
};


#endif