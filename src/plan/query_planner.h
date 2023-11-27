#ifndef QUERY_PLANNER_H
#define QUERY_PLANNER_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <memory>
#include <climits>
#include <cstring>
#include <algorithm>
#include "query_plan.h"
#include "query_decomposer.h"
#include "query/query_graph.h"
#include "database/statistics_manager.h"
#include "util/memory_pool.h"
#include "util/bit_set.h"


class QueryPlanner {
private:
  struct QuerySolution {
    PlanNode* root;
    BitSet nodes;
    QuerySolution(size_t num_of_nodes);
  };

public:
  QueryPlanner(StatisticsManager& stat_manager);
  ~QueryPlanner();

  QueryPlan* build(const QueryGraph& graph);

  static std::unique_ptr<QueryPlan> build(const QueryGraph& graph, StatisticsManager& stat_manager);

private:
  QuerySolution* buildSubGraph(const QueryGraph& graph, const GHDNode& node);
  QuerySolution* buildJoin(const QueryGraph& graph, const QueryPattern* pattern, const std::vector<QuerySolution*>& child_solutions);
  QuerySolution* buildAcyclicJoin(const QueryGraph& graph, const QueryPattern* pattern, const std::vector<QuerySolution*>& child_solutions);
  QuerySolution* buildStarJoin(const QueryGraph& graph, std::vector<QuerySolution*>& child_solutions, uint32_t join_key);
  QuerySolution* buildCyclicJoin(const QueryGraph& graph, std::vector<QuerySolution*>& child_solutions);
  QuerySolution* buildUnion(const QueryGraph& graph, const QueryPattern* pattern);
  QuerySolution* buildScan(const QueryGraph& graph, int node_index);

  PlanNode* createHashJoinNode(const std::vector<PlanNode*>& child_nodes, const std::vector<JoinNode>& join_nodes, const std::vector<uint32_t>& join_res, BitSet& available_res);
  PlanNode* createBackProbeHashJoinNode(const std::vector<PlanNode*>& child_nodes, const std::vector<JoinNode>& join_nodes, const std::vector<uint32_t>& join_res, BitSet& available_res);
  PlanNode* createLeapFrogTrieJoin(const std::vector<PlanNode*>& child_nodes, const std::vector<JoinNode>& join_nodes, const std::vector<uint32_t>& join_res, BitSet& available_res);
  PlanNode* createTableScanNode(int node_index, const QueryNode& query_node, BitSet& available_res);
  PlanNode* createTripleIndexLookupNode(int node_index, const QueryNode& query_node, TripleOrder triple_order, BitSet& available_res);
  PlanNode* createSwitchNode(const std::vector<PlanNode*>& child_nodes);
  PlanNode* createFilterNode(uint32_t filter_key, PlanNode* child);
  PlanNode* createResultsPrinterNode(const QueryGraph& graph, PlanNode* child);
  PlanNode* copyNode(PlanNode* node);

  void integrateBackProbeHashJoin(PlanNode* node, uint32_t join_key, double cardinality);

  void bindResource(PlanNode* node, const std::set<uint32_t>& required_res);

  void optimize(PlanNode* node);
  void adaptiveSwitch(PlanNode* node, std::vector<uint32_t>& join_keys);

  StatisticsManager& stat_manager;
  QueryPlan *plan;

  MemoryPool<QuerySolution> solution_pool;
};


#endif
