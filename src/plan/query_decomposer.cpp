#include "query_decomposer.h"
#include <iostream>


QueryDecomposer::QueryDecomposer() {}


QueryDecomposer::~QueryDecomposer() {}

void QueryDecomposer::decompose(const QueryGraph& graph, std::vector<GHDNode>& nodes) {
  std::vector<int> edges(graph.numOfNodes());
  for(int i=0; i<graph.numOfNodes(); i++) {
    edges[i] = i;
  }
  splitGraph(graph, edges, nodes);
}

void QueryDecomposer::splitGraph(const QueryGraph& graph, const std::vector<int>& edges, std::vector<GHDNode>& nodes) {
  if(edges.size() == 1) {
    nodes.push_back({type: GHDNode::Single, query_nodes: edges});
    return;
  } else if(edges.size() == 2) {
    nodes.push_back({type: GHDNode::Single, query_nodes: {edges[0]}});
    nodes.push_back({type: GHDNode::Single, query_nodes: {edges[1]}});
    return;
  }

  // std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, int>>> vertices;
  std::unordered_map<uint32_t, std::vector<std::tuple<uint32_t, int, int>>> vertices;
  for(int i=0; i<edges.size(); i++) {
    QueryNode node = graph.getNode(edges[i]);
    uint32_t subject_id = node.subject.id;
    uint32_t object_id = node.object.id;
    if(node.subject.type != QueryResource::Variable) {
      subject_id += graph.numOfVariables();
    }
    if(node.object.type != QueryResource::Variable) {
      object_id += graph.numOfVariables();
    }
    //vertices[subject_id].push_back(std::make_pair(object_id, i));
    //vertices[object_id].push_back(std::make_pair(subject_id, i));
    vertices[subject_id].push_back(std::make_tuple(object_id, i, edges[i]));
    vertices[object_id].push_back(std::make_tuple(subject_id, i, edges[i]));
  }
  std::vector<std::pair<double, int>> weighted_edges(edges.size(), std::make_pair(0, 0));
  for (auto i = vertices.begin(); i != vertices.end(); i++) {
    for(int j=0; j<i->second.size(); j++) {
      // weighted_edges[i->second[j].second].first += 1.0/i->second.size();
      // weighted_edges[i->second[j].second].second = i->second[j].second;
      weighted_edges[std::get<1>(i->second[j])].first += 1.0/i->second.size();
      weighted_edges[std::get<1>(i->second[j])].second = std::get<2>(i->second[j]);
    }
  }
  sort(weighted_edges.begin(), weighted_edges.end());
  if(weighted_edges.front().first == weighted_edges.back().first) {
    std::unordered_map<uint32_t, int> keys;
    uint32_t max_key = 0;
    int max_count = 0;
    int count = 0;
    for(int i=0; i<edges.size(); i++) {
      QueryNode node = graph.getNode(edges[i]);
      count = ++keys[node.subject.id];
      if(count > max_count) {
        max_count = count;
        max_key = node.subject.id;
      }
      count = ++keys[node.object.id];
      if(count > max_count) {
        max_count = count;
        max_key = node.object.id;
      }
    }
    if(max_count == edges.size()) {
      nodes.push_back({type: GHDNode::Star, query_nodes: edges, common_key: max_key});
      return;
    }
  }

  bool found = false;
  std::unordered_set<int> left_edges;
  std::unordered_set<int> right_edges;
  for(int i=0; i<weighted_edges.size(); i++) {
    QueryNode node = graph.getNode(weighted_edges[i].second);
    uint32_t left = node.subject.id;
    BitSet left_vars(graph.numOfVariables());
    if(node.subject.type != QueryResource::Variable) {
      left += graph.numOfVariables();
    } else {
      left_vars.set(left);
    }
    left_edges.clear();
    
    uint32_t right = node.object.id;
    BitSet right_vars(graph.numOfVariables());
    if(node.object.type != QueryResource::Variable) {
      right += graph.numOfVariables();
    } else {
      right_vars.set(right);
    }
    right_edges.clear();

    findAllVariables(vertices, left, right, left_vars, left_edges);
    findAllVariables(vertices, right, left, right_vars, right_edges);

    if(!left_vars.intersects(right_vars)) {
      if(left_edges.size() == 0) {
        left_edges.insert(weighted_edges[i].second);
      } else if(right_edges.size() == 0) {
        right_edges.insert(weighted_edges[i].second);
      } else {
        const std::vector<std::tuple<uint32_t, int, int>>& left_neighbors = vertices[left];
        const std::vector<std::tuple<uint32_t, int, int>>& right_neighbors = vertices[right];
        if(left_neighbors.size() >= right_neighbors.size()) {
          left_edges.insert(weighted_edges[i].second);
        } else {
          right_edges.insert(weighted_edges[i].second);
        }
      }
      

      found = true;
      break;
    }
  }

  if(!found) {
    nodes.push_back({type: GHDNode::Cyclic, query_nodes: edges});
    return;
  }

  splitGraph(graph, std::vector<int>(left_edges.begin(), left_edges.end()), nodes);
  splitGraph(graph, std::vector<int>(right_edges.begin(), right_edges.end()), nodes);
}

void QueryDecomposer::findAllVariables(const std::unordered_map<uint32_t, std::vector<std::tuple<uint32_t, int, int>>>& vertices, uint32_t start, uint32_t except, BitSet& variables, std::unordered_set<int>& edges) {
  if(vertices.count(start) == 0) {
    return;
  }
  const std::vector<std::tuple<uint32_t, int, int>>& neighbors = vertices.at(start);
  for(int i=0; i<neighbors.size(); i++) {
    if(std::get<0>(neighbors[i]) == except) {
      continue;
    }
    edges.insert(std::get<2>(neighbors[i]));
    if(std::get<0>(neighbors[i]) > variables.max()) {
      continue;
    }
    if(!variables.test(std::get<0>(neighbors[i]))) {
      variables.set(std::get<0>(neighbors[i]));
      findAllVariables(vertices, std::get<0>(neighbors[i]), except, variables, edges);
    }
  }
}