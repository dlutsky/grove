#include "query_planner.h"
#include "lb_cost_model.h"
#include "util/math_functions.h"


QueryPlanner::QueryPlanner(StatisticsManager& stat_manager) : stat_manager(stat_manager), plan(nullptr) {}

QueryPlanner::~QueryPlanner() { delete plan; }

QueryPlanner::QuerySolution::QuerySolution(size_t num_of_nodes) : nodes(num_of_nodes) {}

QueryPlan* QueryPlanner::build(const QueryGraph& graph) {
  QueryPlan* new_plan = new QueryPlan();
  plan = new_plan;

  QueryDecomposer docomposer;
  std::vector<GHDNode> nodes;
  docomposer.decompose(graph, nodes);
  std::vector<QuerySolution*> node_solutions(nodes.size());
  for(int i=0; i<nodes.size(); i++) {
    /*
    std::cout<<"subgraph "<<i<<" "<<nodes[i].type<<" "<<nodes[i].common_key<<std::endl;
    for(int j=0; j<nodes[i].query_nodes.size(); j++) {
      std::cout<<nodes[i].query_nodes[j]<<" ";
    }
    std::cout<<std::endl;
    */
    node_solutions[i] = buildSubGraph(graph, nodes[i]);
  }

  QuerySolution *solution = buildJoin(graph, graph.getQueryPattern(), node_solutions);

  plan->root = createResultsPrinterNode(graph, solution->root);


  std::set<uint32_t> required_res;
  bindResource(plan->root, required_res);

  optimize(plan->root);

  plan = nullptr;
  return new_plan;
}

std::unique_ptr<QueryPlan> QueryPlanner::build(const QueryGraph& graph, StatisticsManager& stat_manager) {
  QueryPlanner planner(stat_manager);
  return std::unique_ptr<QueryPlan>(planner.build(graph));
}

QueryPlanner::QuerySolution* QueryPlanner::buildSubGraph(const QueryGraph& graph, const GHDNode& node) {
  QuerySolution* solution = solution_pool.alloc();
  if(node.type == GHDNode::Single) {
    solution = buildScan(graph, node.query_nodes[0]);
  } else if(node.type == GHDNode::Star) {
    std::vector<QuerySolution*> sub_solutions;
    for(int i=0; i<node.query_nodes.size(); i++) {
      sub_solutions.push_back(buildScan(graph, node.query_nodes[i]));
    }
    solution = buildStarJoin(graph, sub_solutions, node.common_key);
  } else if(node.type == GHDNode::Cyclic) {
    std::vector<QuerySolution*> sub_solutions;
    for(int i=0; i<node.query_nodes.size(); i++) {
      sub_solutions.push_back(buildScan(graph, node.query_nodes[i]));
    }
    solution = buildCyclicJoin(graph, sub_solutions);
  }
  return solution;
}

QueryPlanner::QuerySolution* QueryPlanner::buildJoin(const QueryGraph& graph, const QueryPattern* pattern, const std::vector<QuerySolution*>& child_solutions) {
  QuerySolution *solution;
  if(child_solutions.size() > 1) {
    solution = buildAcyclicJoin(graph, pattern, child_solutions);
  } else {
    solution = child_solutions[0];
  }

  integrateBackProbeHashJoin(solution->root, UINT32_MAX, 0);

  return solution;
}

QueryPlanner::QuerySolution* QueryPlanner::buildAcyclicJoin(const QueryGraph& graph, const QueryPattern* pattern, const std::vector<QuerySolution*>& child_solutions) {
  std::vector<std::vector<QuerySolution*>> dp_table(child_solutions.size()+1);
  std::map<BitSet, QuerySolution*> lookup;
  for(int i=0; i<child_solutions.size(); i++) {
    dp_table[1].push_back(child_solutions[i]);
  }

  int t = 0;
  for(int i=2; i<=child_solutions.size(); i++) {
    for(int j=0; j<dp_table[i-1].size(); j++) {
      for(int k=0; k<dp_table[1].size(); k++) {
        BitSet node_intersect = dp_table[i-1][j]->nodes & dp_table[1][k]->nodes;
        if(!node_intersect.any()) {
          PlanNode* left = dp_table[i-1][j]->root;
          while(left != nullptr) {
            PlanNode* right = dp_table[1][k]->root;

            BitSet left_res = left->available_res;
            BitSet right_res = right->available_res;

            BitSet res_intersect = left_res & right_res;
            if(!res_intersect.any()) {
              left = left->next;
              continue;
            }
            uint32_t join_key = *res_intersect.begin();

            BitSet node_union = dp_table[i-1][j]->nodes | dp_table[1][k]->nodes;
            QuerySolution *solution;
            bool exist =false;
            if(lookup.count(node_union) != 0) {
              solution = lookup[node_union];
              exist = true;
            } else {
              solution = solution_pool.alloc();
            }

            PlanNode* node = nullptr;

            if(!left->is_subgraph && (left->op == PlanNode::BackProbeHashJoin || left->op == PlanNode::HashJoin)) {
              if((right_res&left->head).any()) {
                std::vector<PlanNode*> child_nodes;
                std::vector<JoinNode> join_nodes;
                std::vector<uint32_t> join_res;

                child_nodes.push_back(right);
                join_nodes.push_back(JoinNode(UINT32_MAX, join_key));
                int m = child_nodes.size();
                
                join_res.push_back(join_key);
                
                child_nodes.insert(child_nodes.end(), left->child_nodes.begin(), left->child_nodes.end());
                join_nodes.insert(join_nodes.end(), left->join_nodes.begin(), left->join_nodes.end());

                BitSet available_res = left_res|right_res;
                join_res.insert(join_res.end(), left->join_res.begin(), left->join_res.end());
                for(int x=0; x<left->child_nodes.size(); ++x) {
                  if(left->child_nodes[x]->available_res.test(join_key)) {
                    join_nodes[x+m].left_join_key = join_key;
                  }
                }
                
                node = createBackProbeHashJoinNode(child_nodes, join_nodes, join_res, available_res);
                node->head = right_res;
                node->tail = left->tail;
              } else if((right_res&left->tail).any()) {
                std::vector<PlanNode*> child_nodes;
                std::vector<JoinNode> join_nodes;
                std::vector<uint32_t> join_res;

                child_nodes.insert(child_nodes.end(), left->child_nodes.begin(), left->child_nodes.end());
                join_nodes.insert(join_nodes.end(), left->join_nodes.begin(), left->join_nodes.end());
                join_nodes.back().right_join_key = join_key;
                join_res.insert(join_res.end(), left->join_res.begin(), left->join_res.end());

                child_nodes.push_back(right);
                join_nodes.push_back(JoinNode(join_key, join_key));
                join_res.push_back(join_key);

                BitSet available_res = left_res|right_res;
                
                node = createBackProbeHashJoinNode(child_nodes, join_nodes, join_res, available_res);
                node->head = left->head;
                node->tail = right_res;
              }

              if(i<=3) {
                if(left->cardinality < right->cardinality) {
                  std::vector<PlanNode*> child_nodes;
                  std::vector<JoinNode> join_nodes;
                  std::vector<uint32_t> join_res;

                  child_nodes.push_back(left);
                  join_nodes.push_back(JoinNode(UINT32_MAX, join_key));
                
                  child_nodes.push_back(right);
                  join_nodes.push_back(JoinNode(join_key, join_key));
                
                  join_res.push_back(join_key);
                
                  BitSet available_res = left_res|right_res;
                
                  PlanNode* hash_node = createHashJoinNode(child_nodes, join_nodes, join_res, available_res);
                  hash_node->head = left_res;
                  hash_node->tail = right_res;
                  if(node == nullptr || hash_node->costs < node->costs) {
                    node = hash_node;
                  }
                } else {
                  std::vector<PlanNode*> child_nodes;
                  std::vector<JoinNode> join_nodes;
                  std::vector<uint32_t> join_res;

                  child_nodes.push_back(right);
                  join_nodes.push_back(JoinNode(UINT32_MAX, join_key));
                
                  join_res.push_back(join_key);
                
                  child_nodes.push_back(left);
                  join_nodes.push_back(JoinNode(join_key, join_key));
                
                  BitSet available_res = left_res|right_res;
                
                  PlanNode* hash_node = createHashJoinNode(child_nodes, join_nodes, join_res, available_res);
                  hash_node->head = right_res;
                  hash_node->tail = left_res;
                  if(node == nullptr || hash_node->costs < node->costs) {
                    node = hash_node;
                  }
                }
              } else {
                left = left->next;
                continue;
              }
            } else {
              if(left->cardinality < right->cardinality) {
                std::vector<PlanNode*> child_nodes;
                std::vector<JoinNode> join_nodes;
                std::vector<uint32_t> join_res;

                child_nodes.push_back(left);
                join_nodes.push_back(JoinNode(UINT32_MAX, join_key));
                
                child_nodes.push_back(right);
                join_nodes.push_back(JoinNode(join_key, join_key));
                
                join_res.push_back(join_key);
                
                BitSet available_res = left_res|right_res;
                
                node = createHashJoinNode(child_nodes, join_nodes, join_res, available_res);
                node->head = left_res;
                node->tail = right_res;
              } else {
                std::vector<PlanNode*> child_nodes;
                std::vector<JoinNode> join_nodes;
                std::vector<uint32_t> join_res;

                child_nodes.push_back(right);
                join_nodes.push_back(JoinNode(UINT32_MAX, join_key));
                
                join_res.push_back(join_key);
                
                child_nodes.push_back(left);
                join_nodes.push_back(JoinNode(join_key, join_key));
                
                BitSet available_res = left_res|right_res;
                
                node = createHashJoinNode(child_nodes, join_nodes, join_res, available_res);
                node->head = right_res;
                node->tail = left_res;
              }
            }

            // prune
            if(exist){
              if(node->costs < solution->root->costs) {
                solution->root = node;
              }
            } else {
              solution->root = node;
              solution->nodes = node_union;
              dp_table[i].push_back(solution);
              t = i;
              lookup[node_union] = solution;
            }

            left = left->next;
          }

        }
      }
    }
  }

  QuerySolution* result = nullptr;
  //t = 2;
  for(int i=0; i<dp_table[t].size(); ++i) {
    PlanNode* join_node = dp_table[t][i]->root;
    PlanNode* best_node = dp_table[t][i]->root;
    while(join_node != nullptr) {
      if(join_node->costs < best_node->costs) {
        best_node = join_node;
      }
      join_node = join_node->next;
    }
    dp_table[t][i]->root = best_node;

    if(result == nullptr) {
      result = dp_table[t][i];
    } else if(dp_table[t][i]->root->costs < result->root->costs) {
      result = dp_table[t][i];
    }
  }

  if(t != child_solutions.size()) {
    std::vector<QuerySolution*> node_solutions;
    node_solutions.push_back(result);
    for(int i=0; i<child_solutions.size(); ++i) {
      BitSet node_intersect = child_solutions[i]->nodes & result->nodes;
      if(!node_intersect.any()) {
        node_solutions.push_back(child_solutions[i]);
      }
    }
    result = buildAcyclicJoin(graph, graph.getQueryPattern(), node_solutions);
  }

  return result;
}

QueryPlanner::QuerySolution* QueryPlanner::buildStarJoin(const QueryGraph& graph, std::vector<QuerySolution*>& child_solutions, uint32_t join_key) {
  QuerySolution* star_solution = solution_pool.alloc();

  struct {
    bool operator()(QuerySolution* a, QuerySolution* b) const { return a->root->cardinality < b->root->cardinality; }
  } customLess;
  std::sort(child_solutions.begin(), child_solutions.end(), customLess);

  PlanNode::Operator join_op = PlanNode::BackProbeHashJoin;
  // PlanNode::Operator join_op = PlanNode::LeapFrogTrieJoin;

  star_solution->nodes = BitSet(graph.numOfNodes());
  std::vector<JoinNode> join_nodes;
  std::vector<uint32_t> join_res;

  BitSet available_res(graph.numOfVariables());
  std::vector<PlanNode*> child_nodes;
  for(int i=0; i<child_solutions.size(); i++) {
    star_solution->nodes |= child_solutions[i]->nodes;
    if(child_solutions[i]->root->query_node.subject.type == QueryResource::Variable) {
      available_res.set(child_solutions[i]->root->query_node.subject.id);
    }
    if(child_solutions[i]->root->query_node.object.type == QueryResource::Variable) {
      available_res.set(child_solutions[i]->root->query_node.object.id);
    }

    PlanNode* child_node = child_solutions[i]->root;

    ResourcePosition join_pos;
    if(join_key == child_node->query_node.subject.id) {
      join_pos = ResourcePosition::SUBJECT;
    } else if (join_key == child_node->query_node.object.id) {
      join_pos = ResourcePosition::OBJECT;
    }
    
    if(join_op == PlanNode::LeapFrogTrieJoin) {
      child_node->op = PlanNode::TripleIndexLookup;
      child_node->ordering = join_pos;
      child_nodes.push_back(child_node);

      join_nodes.push_back(JoinNode({join_key}));
    } else {
      child_nodes.push_back(child_node);
      if(i==0) {
        join_nodes.push_back(JoinNode(UINT32_MAX, join_key));
      } else {
        join_nodes.push_back(JoinNode(join_key, join_key));
      }
    }
  }
  join_res.push_back(join_key);

  PlanNode* node;

  if(join_op == PlanNode::LeapFrogTrieJoin) {
    node = createLeapFrogTrieJoin(child_nodes, join_nodes, join_res, available_res);
  } else {
    node = createBackProbeHashJoinNode(child_nodes, join_nodes, join_res, available_res);
  }

  node->is_subgraph = true;
  node->is_star = true;

  star_solution->root = node;
  // std::cout<<node->cardinality<<" "<<node->costs<<std::endl;

  return star_solution;
}

QueryPlanner::QuerySolution* QueryPlanner::buildCyclicJoin(const QueryGraph& graph, std::vector<QuerySolution*>& child_solutions) {
  QuerySolution* cyclic_solution = solution_pool.alloc();

  struct {
    bool operator()(QuerySolution* a, QuerySolution* b) const { return a->root->cardinality < b->root->cardinality; }
  } customLess;
  std::sort(child_solutions.begin(), child_solutions.end(), customLess);

  cyclic_solution->nodes = BitSet(graph.numOfNodes());
  std::vector<JoinNode> join_nodes;
  std::vector<uint32_t> join_res;

  BitSet available_res(graph.numOfVariables());
  std::vector<PlanNode*> child_nodes;
  std::set<int> checked;
  PlanNode* child_node = child_solutions[0]->root;
  cyclic_solution->nodes |= child_solutions[0]->nodes;
  child_nodes.push_back(child_node);
  checked.insert(0);
  BitSet prev_res = child_node->available_res;
  int j = 1;
  for(int i=1; i<child_solutions.size(); i++) {
    if(checked.count(i) != 0) {
      continue;
    }
    child_node = child_solutions[i]->root;
    BitSet res_intersect = prev_res & child_node->available_res;
    if(res_intersect.any()) {
      child_nodes.push_back(child_node);
      checked.insert(i);
      cyclic_solution->nodes |= child_solutions[i]->nodes;
      if(i == j) {
        j = i+1;
      } else {
        i = j-1;
      }
    }
  }
  bool subject_join, object_join;
  for(int i=0; i<child_nodes.size(); i++) {
    subject_join = false;
    object_join =false;
    if(child_nodes[i]->query_node.subject.type == QueryResource::Variable) {
      if(available_res.test(child_nodes[i]->query_node.subject.id)) {
        join_res.push_back(child_nodes[i]->query_node.subject.id);
        subject_join = true;
      } else {
        available_res.set(child_nodes[i]->query_node.subject.id);
      }
    }
    if(child_nodes[i]->query_node.object.type == QueryResource::Variable) {
      if(available_res.test(child_nodes[i]->query_node.object.id)) {
        join_res.push_back(child_nodes[i]->query_node.object.id);
        object_join = true;
      } else {
        available_res.set(child_nodes[i]->query_node.object.id);
      }
    }
    if(subject_join) {
      if(object_join) {
        child_nodes[i]->triple_order = TripleOrder::PSO;
      } else {
        if(child_nodes[i]->triple_order == TripleOrder::PO) {
          child_nodes[i]->triple_order = TripleOrder::POS;
        } else {
          child_nodes[i]->triple_order = TripleOrder::PS;
        }
      }
    } else {
      if(object_join) {
        if(child_nodes[i]->triple_order == TripleOrder::PS) {
          child_nodes[i]->triple_order = TripleOrder::PSO;
        } else {
          child_nodes[i]->triple_order = TripleOrder::PO;
        }
      }
    }
  }
  for(int i=0; i<child_nodes.size(); i++) {
    child_nodes[i]->op = PlanNode::TripleIndexLookup;

    std::vector<uint32_t> join_keys;
    for(int j=0; j<join_res.size(); j++) {
      if(child_nodes[i]->query_node.subject.id == join_res[j]) {
        join_keys.push_back(join_res[j]);
      } else if(child_nodes[i]->query_node.object.id == join_res[j]) {
        join_keys.push_back(join_res[j]);
      }
    }
    join_nodes.push_back(JoinNode(join_keys));

    // Todo join order
  }

  PlanNode* node = createLeapFrogTrieJoin(child_nodes, join_nodes, join_res, available_res);
  node->is_subgraph = true;

  cyclic_solution->root = node;

  return cyclic_solution;
}


QueryPlanner::QuerySolution* QueryPlanner::buildScan(const QueryGraph& graph, int node_index) {
  QuerySolution *solution = solution_pool.alloc();
  solution->nodes = BitSet(graph.numOfNodes());
  solution->nodes.set(node_index);

  const QueryNode& query_node = graph.getNode(node_index);

  if(query_node.subject.type != QueryResource::Variable) {
    BitSet available_res(graph.numOfVariables());
    available_res.set(query_node.object.id);
    solution->root = createTripleIndexLookupNode(node_index, query_node, TripleOrder::PS, available_res);
    solution->root->ordering = ResourcePosition::OBJECT;
  } else if(query_node.object.type != QueryResource::Variable) {
    BitSet available_res(graph.numOfVariables());
    available_res.set(query_node.subject.id);
    solution->root = createTripleIndexLookupNode(node_index, query_node, TripleOrder::PO, available_res);
    solution->root->ordering = ResourcePosition::SUBJECT;
  } else {
    BitSet available_res(graph.numOfVariables());
    available_res.set(query_node.subject.id);
    available_res.set(query_node.object.id);
    solution->root = createTableScanNode(node_index, query_node, available_res);
  }

  return solution;
}

PlanNode* QueryPlanner::createHashJoinNode(const std::vector<PlanNode*>& child_nodes, const std::vector<JoinNode>& join_nodes, const std::vector<uint32_t>& join_res, BitSet& available_res) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::HashJoin;
  node->next = nullptr;
  node->child_nodes = child_nodes;
  node->join_nodes = join_nodes;
  node->join_res = join_res;
  node->available_res = available_res;
  node->densities = std::map<uint32_t, double>();
  for(int i=0; i<child_nodes.size(); ++i) {
    for (std::map<uint32_t, double>::iterator it = child_nodes[i]->densities.begin(); it != child_nodes[i]->densities.end(); ++it) {
      if(node->densities.count(it->first)) {
        node->densities[it->first] = std::max(node->densities[it->first], it->second);
      } else {
        node->densities[it->first] = it->second;
      }
    }
  }
  uint32_t join_key = join_res[0];
  /*
  if(node->child_nodes[0]->query_node.predicate.id == node->child_nodes[1]->query_node.predicate.id) {
    node->cardinality = std::min(node->child_nodes[1]->cardinality, node->child_nodes[0]->cardinality*node->child_nodes[0]->densities[join_key]);
  } else {
    node->cardinality = std::min(node->child_nodes[1]->cardinality, node->child_nodes[0]->cardinality*node->child_nodes[0]->densities[join_key]/node->child_nodes[1]->densities[join_key]);
  }
  */
  node->cardinality = std::min(node->child_nodes[1]->cardinality, node->child_nodes[0]->cardinality*node->child_nodes[0]->densities[join_key]/node->child_nodes[1]->densities[join_key]);
  // Todo: calculate costs
  node->costs = node->child_nodes[0]->costs + node->child_nodes[0]->cardinality + node->child_nodes[1]->costs + std::min(child_nodes[1]->cardinality, node->cardinality*20);
  return node;
}

PlanNode* QueryPlanner::createBackProbeHashJoinNode(const std::vector<PlanNode*>& child_nodes, const std::vector<JoinNode>& join_nodes, const std::vector<uint32_t>& join_res, BitSet& available_res) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::BackProbeHashJoin;
  node->next = nullptr;
  node->child_nodes = child_nodes;
  node->join_nodes = join_nodes;
  node->join_res = join_res;
  node->available_res = available_res;
  node->costs = child_nodes[0]->costs + child_nodes[0]->cardinality;
  std::unordered_map<uint32_t, double> distinct_counts;
  for(BitSet::SetBitIterator iter = child_nodes[0]->available_res.begin(); iter != child_nodes[0]->available_res.end(); ++iter) {
    distinct_counts[*iter] = child_nodes[0]->cardinality*child_nodes[0]->densities[*iter];
  }
  double cardinality = child_nodes[0]->cardinality;
  for(int x=1; x<child_nodes.size(); ++x) {
    node->costs += child_nodes[x]->costs + std::min(child_nodes[x]->cardinality, cardinality*20);
    // node->costs += child_nodes[x]->costs + cardinality;
    cardinality = child_nodes[x]->cardinality;
    for(BitSet::SetBitIterator iter = child_nodes[x]->available_res.begin(); iter != child_nodes[x]->available_res.end(); ++iter) {
      if(distinct_counts.count(*iter)) {
        cardinality = std::min(cardinality, distinct_counts[*iter]/child_nodes[x]->densities[*iter]);
      }
    }
    for(BitSet::SetBitIterator iter = child_nodes[x]->available_res.begin(); iter != child_nodes[x]->available_res.end(); ++iter) {
      if(distinct_counts.count(*iter)) {
        distinct_counts[*iter] = std::min(distinct_counts[*iter], cardinality*child_nodes[x]->densities[*iter]);
      } else {
        distinct_counts[*iter] = cardinality*child_nodes[x]->densities[*iter];
      }
    }
  }
  node->cardinality = cardinality;
  node->densities = std::map<uint32_t, double>();
  for(int i=0; i<child_nodes.size(); ++i) {
    for (std::map<uint32_t, double>::iterator it = child_nodes[i]->densities.begin(); it != child_nodes[i]->densities.end(); ++it) {
      if(node->densities.count(it->first)) {
        // node->densities[it->first] = std::min(node->densities[it->first], it->second*child_nodes[i]->cardinality/node->cardinality);
        node->densities[it->first] = std::min(node->densities[it->first], it->second);
      } else {
        // node->densities[it->first] = it->second*child_nodes[i]->cardinality/node->cardinality;
        node->densities[it->first] = it->second;
      }
    }
  }
  return node;
}

PlanNode* QueryPlanner::createLeapFrogTrieJoin(const std::vector<PlanNode*>& child_nodes, const std::vector<JoinNode>& join_nodes, const std::vector<uint32_t>& join_res, BitSet& available_res) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::LeapFrogTrieJoin;
  node->next = nullptr;
  node->child_nodes = child_nodes;
  node->join_nodes = join_nodes;
  node->join_res = join_res;
  node->available_res = available_res;
  node->densities = std::map<uint32_t, double>();
  for(int i=0; i<child_nodes.size(); ++i) {
    for (std::map<uint32_t, double>::iterator it = child_nodes[i]->densities.begin(); it != child_nodes[i]->densities.end(); ++it) {
      if(node->densities.count(it->first)) {
        node->densities[it->first] = std::max(node->densities[it->first], it->second);
      } else {
        node->densities[it->first] = it->second;
      }
    }
  }
  node->costs = child_nodes[0]->costs + child_nodes[0]->cardinality;
  std::unordered_map<uint32_t, double> distinct_counts;
  for(BitSet::SetBitIterator iter = child_nodes[0]->available_res.begin(); iter != child_nodes[0]->available_res.end(); ++iter) {
    distinct_counts[*iter] = child_nodes[0]->cardinality*child_nodes[0]->densities[*iter];
  }
  double cardinality = child_nodes[0]->cardinality;
  for(int x=1; x<child_nodes.size(); ++x) {
    node->costs += child_nodes[x]->costs + std::min(child_nodes[x]->cardinality, cardinality*20);
    // node->costs += child_nodes[x]->costs + cardinality;
    cardinality = child_nodes[x]->cardinality;
    for(BitSet::SetBitIterator iter = child_nodes[x]->available_res.begin(); iter != child_nodes[x]->available_res.end(); ++iter) {
      if(distinct_counts.count(*iter)) {
        cardinality = std::min(cardinality, distinct_counts[*iter]/child_nodes[x]->densities[*iter]);
      }
    }
    for(BitSet::SetBitIterator iter = child_nodes[x]->available_res.begin(); iter != child_nodes[x]->available_res.end(); ++iter) {
      if(distinct_counts.count(*iter)) {
        distinct_counts[*iter] = std::min(distinct_counts[*iter], cardinality*child_nodes[x]->densities[*iter]);
      } else {
        distinct_counts[*iter] = cardinality*child_nodes[x]->densities[*iter];
      }
    }
  }
  node->cardinality = cardinality;
  node->densities = std::map<uint32_t, double>();
  for(int i=0; i<child_nodes.size(); ++i) {
    for (std::map<uint32_t, double>::iterator it = child_nodes[i]->densities.begin(); it != child_nodes[i]->densities.end(); ++it) {
      if(node->densities.count(it->first)) {
        // node->densities[it->first] = std::min(node->densities[it->first], it->second*child_nodes[i]->cardinality/node->cardinality);
        node->densities[it->first] = std::min(node->densities[it->first], it->second);
      } else {
        // node->densities[it->first] = it->second*child_nodes[i]->cardinality/node->cardinality;
        node->densities[it->first] = it->second;
      }
    }
  }
  return node;
}

PlanNode* QueryPlanner::createTableScanNode(int node_index, const QueryNode& query_node, BitSet& available_res) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::TableScan;
  node->next = nullptr;
  node->node_index = node_index;
  node->query_node = query_node;
  node->num_of_bounds = 1;
  node->ordering = ResourcePosition::PREDICATE;

  node->triple_order = TripleOrder::P;
  node->cardinality = stat_manager.getCount(TripleOrder::P, 0, query_node.predicate.id, 0);
  node->costs = LowerBoundsCostModel::estimateTableScan((node->cardinality*sizeof(uint32_t)*2+BLOCK_SIZE)/BLOCK_SIZE);
  node->densities = std::map<uint32_t, double>();
  node->densities[query_node.subject.id] = 1.0*stat_manager.getDistinctCount(TripleOrder::SP, 0, query_node.predicate.id, 0)/node->cardinality;
  node->densities[query_node.object.id] = 1.0*stat_manager.getDistinctCount(TripleOrder::OP, 0, query_node.predicate.id, 0)/node->cardinality;
  node->available_res = available_res;
  
  return node;
}

PlanNode* QueryPlanner::createTripleIndexLookupNode(int node_index, const QueryNode& query_node, TripleOrder triple_order, BitSet& available_res) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::TripleIndexLookup;
  node->next = nullptr;
  node->node_index = node_index;
  node->query_node = query_node;
  node->triple_order = triple_order;

  switch(triple_order) {
    case TripleOrder::SP:
    case TripleOrder::PS:
      node->cardinality = stat_manager.getCount(triple_order, query_node.subject.id, query_node.predicate.id, 0);
      node->densities = std::map<uint32_t, double>();
      node->densities[query_node.object.id] = 1.0*stat_manager.getDistinctCount(TripleOrder::OP, 0, query_node.predicate.id, 0)/stat_manager.getCount(TripleOrder::P, 0, query_node.predicate.id, 0);
      // std::cout<<node->densities[query_node.object.id]<<std::endl;
      node->num_of_bounds = 2;
      node->ordering = ResourcePosition::SUBJECT;
      break;
    case TripleOrder::OP:
    case TripleOrder::PO:
      node->cardinality = stat_manager.getCount(triple_order, 0, query_node.predicate.id, query_node.object.id);
      node->densities = std::map<uint32_t, double>();
      node->densities[query_node.subject.id] = 1.0*stat_manager.getDistinctCount(TripleOrder::SP, 0, query_node.predicate.id, 0)/stat_manager.getCount(TripleOrder::P, 0, query_node.predicate.id, 0);
      // std::cout<<node->densities[query_node.subject.id]<<std::endl;
      node->num_of_bounds = 2;
      node->ordering = ResourcePosition::OBJECT;
      break;
  }
  node->costs = LowerBoundsCostModel::estimateTableScan((node->cardinality*sizeof(uint32_t)+BLOCK_SIZE)/BLOCK_SIZE);
  node->available_res = available_res;
  return node;
}

PlanNode* QueryPlanner::createSwitchNode(const std::vector<PlanNode*>& child_nodes) {
  if(child_nodes.empty()) {
    return nullptr;
  }
  PlanNode* child_node = child_nodes[0];

  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::Switch;
  node->next = nullptr;
  node->child_nodes = child_nodes;
  node->available_res = child_node->available_res;
  node->costs = child_node->costs;
  node->cardinality = child_node->cardinality;
  return node;
}

PlanNode* QueryPlanner::createFilterNode(uint32_t filter_key, PlanNode* child) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::Filter;
  node->next = nullptr;
  node->node_index = child->node_index;
  node->query_node = child->query_node;
  node->child_nodes.push_back(child);
  node->filter_key = filter_key;
  node->available_res = child->available_res;
  node->costs = child->costs + child->cardinality;
  node->cardinality = child->cardinality;

  
  return node;
}

PlanNode* QueryPlanner::createResultsPrinterNode(const QueryGraph& graph, PlanNode* child) {
  PlanNode* node = plan->createPlanNode();
  node->op = PlanNode::ResultsPrinter;
  node->next = nullptr;
  node->child_nodes.push_back(child);
  node->projection = graph.getProjection();
  node->costs = child->costs + child->cardinality;
  node->cardinality = child->cardinality;
  return node;
}

PlanNode* QueryPlanner::copyNode(PlanNode* node) {
  PlanNode* new_node = plan->createPlanNode();
  new_node->op = node->op;
  new_node->next = node->next;

  new_node->child_nodes = node->child_nodes;
  new_node->required_res = node->required_res;
  new_node->available_res = node->available_res;

  new_node->node_index = node->node_index;
  new_node->query_node = node->query_node;
  new_node->triple_order = node->triple_order;
  new_node->ordering = node->ordering;
  new_node->filter_key = node->filter_key;
  
  new_node->costs = node->costs;
  new_node->cardinality = node->cardinality;
  new_node->densities = node->densities;
  return new_node;
}

void QueryPlanner::integrateBackProbeHashJoin(PlanNode* node, uint32_t join_key, double cardinality) {
  if(node->op == PlanNode::BackProbeHashJoin) {
    if(node->is_star && join_key != UINT32_MAX) {
      int j = 0;
      for(int i=0; i<node->child_nodes.size(); i++) {
        if(node->child_nodes[i]->available_res.test(join_key)) {
          j = i;
          break;
        }
      }
      int k = j;
      for(int i=j; i>=0; i--) {
        if(node->child_nodes[i]->cardinality > cardinality) {
          k = i;
        }
      }
      if(k<j) {
        std::rotate(node->child_nodes.rend() - j - 1, node->child_nodes.rend() - j, node->child_nodes.rend() - k);
      }
    }
    for(int i=0; i<node->child_nodes.size(); i++) {
      PlanNode* child_node = node->child_nodes[i];
      if(child_node->op == PlanNode::HashJoin || child_node->op == PlanNode::BackProbeHashJoin || child_node->op == PlanNode::LeapFrogTrieJoin) {
        continue;
      }
      if(node->join_nodes[i].left_join_key != UINT32_MAX && node->join_nodes[i].left_join_key != node->join_nodes[i].right_join_key) {
        node->child_nodes[i] = createFilterNode(node->join_nodes[i].left_join_key, child_node);
      }
    }
    integrateBackProbeHashJoin(node->child_nodes[0], join_key, cardinality);
    for(int i=1; i<node->child_nodes.size(); i++) {
      integrateBackProbeHashJoin(node->child_nodes[i], node->join_nodes[i-1].right_join_key, node->child_nodes[i-1]->cardinality);
    }
  } else if(node->op == PlanNode::HashJoin || node->op == PlanNode::LeapFrogTrieJoin) {
    integrateBackProbeHashJoin(node->child_nodes[0], join_key, cardinality);
    for(int i=1; i<node->child_nodes.size(); i++) {
      integrateBackProbeHashJoin(node->child_nodes[i], node->join_nodes[i-1].right_join_key, node->child_nodes[i-1]->cardinality);
    }
  } else {
    for(int i=0; i<node->child_nodes.size(); i++) {
      integrateBackProbeHashJoin(node->child_nodes[i], join_key, cardinality);
    }
  }
}

void QueryPlanner::bindResource(PlanNode* node, const std::set<uint32_t>& required_res) {
  node->required_res = required_res;
  std::set<uint32_t> res = required_res;
  if(node->op == PlanNode::ResultsPrinter) {
    for(int i=0; i<node->projection.size(); i++) {
      res.insert(node->projection[i].id);
    }
  } else if(node->op == PlanNode::HashJoin || node->op == PlanNode::BackProbeHashJoin) {
    for(int i=1; i<node->join_nodes.size(); i++) {
      if(node->join_nodes[i].left_join_key != UINT32_MAX) {
        res.insert(node->join_nodes[i].left_join_key);
      }
      if(node->join_nodes[i].right_join_key != UINT32_MAX) {
        res.insert(node->join_nodes[i].right_join_key);
      }
    }
  } else if(node->op == PlanNode::LeapFrogTrieJoin) {
    for(int i=1; i<node->join_nodes.size(); i++) {
      for(int j=0; j<node->join_nodes[i].join_keys.size(); j++) {
        res.insert(node->join_nodes[i].join_keys[j]);
      }
    }
  }

  for(int i=0; i<node->child_nodes.size(); i++) {
    bindResource(node->child_nodes[i], res);
  }
}

void QueryPlanner::optimize(PlanNode* node) {
  std::vector<uint32_t> join_keys;
  adaptiveSwitch(node, join_keys);
}

void QueryPlanner::adaptiveSwitch(PlanNode* node, std::vector<uint32_t>& join_keys) {
  if(node->op == PlanNode::HashJoin || node->op == PlanNode::BackProbeHashJoin) {
    for(int i=0; i<node->child_nodes.size(); i++) {
      PlanNode* child_node = node->child_nodes[i];
      if(node->join_nodes[i].left_join_key != UINT32_MAX) {
        join_keys.push_back(node->join_nodes[i].left_join_key);
      }

      adaptiveSwitch(child_node, join_keys);

      if(node->join_nodes[i].left_join_key != UINT32_MAX) {
        join_keys.pop_back();
      }
    }
  } else if(node->op == PlanNode::LeapFrogTrieJoin) {
    return;
  } else if(node->op == PlanNode::Filter) {
    std::vector<PlanNode*> child_nodes;
    child_nodes.push_back(copyNode(node));
    bool find = false;
    for(int i=0; i<join_keys.size(); ++i) {
      if(node->filter_key == join_keys[i]) {
        find = true;
        PlanNode* child_node = node->child_nodes[0];
        if(child_node->op == PlanNode::TableScan) {
          if(node->query_node.subject.id == join_keys[i]) {
            PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::PS, node->available_res);
            new_node->required_res = node->required_res;
            child_nodes.push_back(new_node);
          } else if (node->query_node.object.id == join_keys[i]) {
            PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::PO, node->available_res);
            new_node->required_res = node->required_res;
            child_nodes.push_back(new_node);
          }
        } else if(child_node->op == PlanNode::TripleIndexLookup) {
          if(node->query_node.subject.id == join_keys[i] && child_node->triple_order == TripleOrder::PO) {
            PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::POS, node->available_res);
            new_node->required_res = node->required_res;
            child_nodes.push_back(new_node);
          } else if(node->query_node.object.id == join_keys[i] && child_node->triple_order == TripleOrder::PS) {
            PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::PSO, node->available_res);
            new_node->required_res = node->required_res;
            child_nodes.push_back(new_node);
          }
        }
      }
    }
    if(child_nodes.size() > 1) {
      node->op = PlanNode::Switch;
      node->child_nodes = child_nodes;
    }
    if(!find) {
      for(int i=0; i<node->child_nodes.size(); i++) {
        adaptiveSwitch(node->child_nodes[i], join_keys);
      }
    }
  } else if(node->op == PlanNode::TableScan) {
    std::vector<PlanNode*> child_nodes;
    child_nodes.push_back(copyNode(node));
    for(int i=0; i<join_keys.size(); ++i) {
      if(node->query_node.subject.id == join_keys[i]) {
        PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::PS, node->available_res);
        new_node->required_res = node->required_res;
        child_nodes.push_back(new_node);
      } else if (node->query_node.object.id == join_keys[i]) {
        PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::PO, node->available_res);
        new_node->required_res = node->required_res;
        child_nodes.push_back(new_node);
      }
    }
    if(child_nodes.size() > 1) {
      node->op = PlanNode::Switch;
      node->child_nodes = child_nodes;
    }
  } else if(node->op == PlanNode::TripleIndexLookup) {
    std::vector<PlanNode*> child_nodes;
    child_nodes.push_back(copyNode(node));
    for(int i=0; i<join_keys.size(); ++i) {
      if(node->query_node.subject.id == join_keys[i] && node->triple_order == TripleOrder::PO) {
        PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::POS, node->available_res);
        new_node->required_res = node->required_res;
        child_nodes.push_back(new_node);
      } else if(node->query_node.object.id == join_keys[i] && node->triple_order == TripleOrder::PS) {
        PlanNode* new_node = createTripleIndexLookupNode(node->node_index, node->query_node, TripleOrder::PSO, node->available_res);
        new_node->required_res = node->required_res;
        child_nodes.push_back(new_node);
      }
    }
    if(child_nodes.size() > 1) {
      node->op = PlanNode::Switch;
      node->child_nodes = child_nodes;
    }
  } else {
    for(int i=0; i<node->child_nodes.size(); i++) {
      adaptiveSwitch(node->child_nodes[i], join_keys);
    }
  }
}
