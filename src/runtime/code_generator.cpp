#include "code_generator.h"
#include "operator/filter.h"
#include "operator/hash_join.h"
#include "operator/backprobe_hash_join.h"
#include "operator/leapfrog_trie_join.h"
#include "operator/table_scan.h"
#include "operator/triple_index_lookup.h"
#include "operator/switch.h"
#include "operator/results_printer.h"

#include <iostream>


CodeGenerator::CodeGenerator(Runtime& runtime, bool silent) : runtime(runtime), silent(silent) {}

CodeGenerator::~CodeGenerator() {}

Operator* CodeGenerator::generate(const PlanNode* plan_node) {
  std::map<uint32_t, Resource*> resources;
  return generateInternal(plan_node, resources);
}

Operator* CodeGenerator::generate(Runtime& runtime, const PlanNode* plan_node, bool silent) {
  CodeGenerator generator(runtime, silent);
  return generator.generate(plan_node);
}

Operator* CodeGenerator::generateInternal(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  switch(plan_node->op) {
    case PlanNode::ResultsPrinter:
      return generateResultsPrinter(plan_node, resources);
    case PlanNode::HashJoin:
      return generateHashJoin(plan_node, resources);
      // return generateBackProbeHashJoin(plan_node, resources);
    case PlanNode::BackProbeHashJoin:
      return generateBackProbeHashJoin(plan_node, resources);
    case PlanNode::LeapFrogTrieJoin:
      return generateLeapFrogTrieJoin(plan_node, resources);
    case PlanNode::TableScan:
      return generateTableScan(plan_node, resources);
    case PlanNode::TripleIndexLookup:
      return generateTripleIndexLookup(plan_node, resources);
    case PlanNode::Switch:
      return generateSwitch(plan_node, resources);
    case PlanNode::Filter:
      return generateFilter(plan_node, resources);
  }
  return nullptr;
}

Operator* CodeGenerator::generateResultsPrinter(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  Operator* child = generateInternal(plan_node->child_nodes[0], resources);

  std::vector<std::string> projection(plan_node->projection.size());
  std::vector<Resource*> rcs(plan_node->projection.size());
  for(int i=0; i<plan_node->projection.size(); i++) {
    projection[i] = plan_node->projection[i].value;
    rcs[i] = resources[plan_node->projection[i].id];
  }
  Operator* opt = new ResultsPrinter(runtime.db.getDictionary(), projection, child, rcs, runtime.getOstream(), plan_node->cardinality, this->silent);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateHashJoin(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  std::map<uint32_t, Resource*> left_rcs;
  Operator* left = generateInternal(plan_node->child_nodes[0], left_rcs);

  std::map<uint32_t, Resource*> right_rcs;
  Operator* right = generateInternal(plan_node->child_nodes[1], right_rcs);

  // output resources
  std::map<uint32_t, Resource*> res;
  for(std::map<uint32_t, Resource*>::iterator iter = right_rcs.begin(); iter != right_rcs.end(); ++iter) {
    if(res.count(iter->first) != 0) {
      continue;
    }
    if(plan_node->required_res.count(iter->first) != 0) {
      Resource* r = runtime.createResource();
      res[iter->first] = r;
    }
  }
  for(std::map<uint32_t, Resource*>::iterator iter = left_rcs.begin(); iter != left_rcs.end(); ++iter) {
    if(res.count(iter->first) != 0) {
      continue;
    }
    if(plan_node->required_res.count(iter->first) != 0) {
      Resource* r = runtime.createResource();
      res[iter->first] = r;
    }
  }

  resources.insert(res.begin(), res.end());

  Operator* opt = new HashJoin(left, left_rcs, right, right_rcs, res, plan_node->join_nodes, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateBackProbeHashJoin(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  std::vector<Operator*> opts;
  // input resources
  std::vector<std::map<uint32_t, Resource*>> rcs;
  for(int i=0; i<plan_node->child_nodes.size(); i++) {
    std::map<uint32_t, Resource*> r;
    opts.push_back(generateInternal(plan_node->child_nodes[i], r));
    rcs.push_back(r);
  }

  // output resources
  std::map<uint32_t, Resource*> res;
  for(int i=rcs.size()-1; i>=0; i--) {
    for(std::map<uint32_t, Resource*>::iterator iter = rcs[i].begin(); iter != rcs[i].end(); ++iter) {
      if(res.count(iter->first) != 0) {
        continue;
      }
      if(plan_node->required_res.count(iter->first) != 0) {
        Resource* r = runtime.createResource();
        res[iter->first] = r;
      }
    }
  }

  resources.insert(res.begin(), res.end());

  Operator* opt = new BackProbeHashJoin(opts, rcs, res, plan_node->join_nodes, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateLeapFrogTrieJoin(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  std::vector<Operator*> opts;
  // input resources
  std::vector<std::map<uint32_t, Resource*>> rcs;
  for(int i=0; i<plan_node->child_nodes.size(); i++) {
    std::map<uint32_t, Resource*> r;
    opts.push_back(generateInternal(plan_node->child_nodes[i], r));
    rcs.push_back(r);
  }

  // output resources
  std::map<uint32_t, Resource*> res;
  for(int i=rcs.size()-1; i>=0; i--) {
    for(std::map<uint32_t, Resource*>::iterator iter = rcs[i].begin(); iter != rcs[i].end(); ++iter) {
      if(res.count(iter->first) != 0) {
        continue;
      }
      if(plan_node->required_res.count(iter->first) != 0) {
        Resource* r = runtime.createResource();
        res[iter->first] = r;
      }
    }
  }

  resources.insert(res.begin(), res.end());

  Operator* opt = new LeapFrogTrieJoin(opts, rcs, res, plan_node->join_nodes, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateTableScan(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  Resource* subject = nullptr;
  Resource* predicate = nullptr;
  Resource* object = nullptr;

  if(resources.count(plan_node->query_node.subject.id) != 0) {
    subject = resources[plan_node->query_node.subject.id];
  } else {
    subject = runtime.createResource();
    if(plan_node->query_node.subject.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.subject.id) != 0) {
      resources[plan_node->query_node.subject.id] = subject;
    }
  }

  if(resources.count(plan_node->query_node.predicate.id) != 0) {
    predicate = resources[plan_node->query_node.predicate.id];
  } else {
    predicate = runtime.createResource();
    if(plan_node->query_node.predicate.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.predicate.id) != 0) {
      resources[predicate->id] = predicate;
    }
  }
  predicate->id = plan_node->query_node.predicate.id;
  predicate->literal = plan_node->query_node.predicate.value;

  if(resources.count(plan_node->query_node.object.id) != 0) {
    object = resources[plan_node->query_node.object.id];
  } else {
    object = runtime.createResource();
    if(plan_node->query_node.object.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.object.id) != 0) {
      resources[plan_node->query_node.object.id] = object;
    }
  }

  Operator* opt = new TableScan(runtime.db.getTripleTable(), plan_node->triple_order, subject, predicate, object, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateTripleIndexLookup(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  Resource* subject = nullptr;
  Resource* predicate = nullptr;
  Resource* object = nullptr;

  TripleOrder index_order;

  switch(plan_node->triple_order) {
    case TripleOrder::P:
      if(plan_node->ordering == ResourcePosition::SUBJECT) {
        index_order = TripleOrder::PS;
      } else if(plan_node->ordering == ResourcePosition::OBJECT) {
        index_order = TripleOrder::PO;
      }

      if(resources.count(plan_node->query_node.subject.id) != 0) {
        subject = resources[plan_node->query_node.subject.id];
      } else {
        subject = runtime.createResource();
        if(plan_node->query_node.subject.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.subject.id) != 0) {
          resources[plan_node->query_node.subject.id] = subject;
        }
      }
      subject->id = 0;

      if(resources.count(plan_node->query_node.predicate.id) != 0) {
        predicate = resources[plan_node->query_node.predicate.id];
      } else {
        predicate = runtime.createResource();
        if(plan_node->query_node.predicate.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.predicate.id) != 0) {
          resources[predicate->id] = predicate;
        }
      }
      predicate->id = plan_node->query_node.predicate.id;
      predicate->literal = plan_node->query_node.predicate.value;

      if(resources.count(plan_node->query_node.object.id) != 0) {
        object = resources[plan_node->query_node.object.id];
      } else {
        object = runtime.createResource();
        if(plan_node->query_node.object.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.object.id) != 0) {
          resources[plan_node->query_node.object.id] = object;
        }
      }
      object->id = 0;
      break;
    case TripleOrder::SP:
    case TripleOrder::PS:
    case TripleOrder::PSO:
      index_order = plan_node->triple_order;

      if(resources.count(plan_node->query_node.subject.id) != 0) {
        subject = resources[plan_node->query_node.subject.id];
      } else {
        subject = runtime.createResource();
        if(plan_node->query_node.subject.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.subject.id) != 0) {
          resources[plan_node->query_node.subject.id] = subject;
        }
      }
      subject->id = plan_node->query_node.subject.id;
      subject->literal = plan_node->query_node.subject.value;

      if(resources.count(plan_node->query_node.predicate.id) != 0) {
        predicate = resources[plan_node->query_node.predicate.id];
      } else {
        predicate = runtime.createResource();
        if(plan_node->query_node.predicate.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.predicate.id) != 0) {
          resources[predicate->id] = predicate;
        }
      }
      predicate->id = plan_node->query_node.predicate.id;
      predicate->literal = plan_node->query_node.predicate.value;

      if(resources.count(plan_node->query_node.object.id) != 0) {
        object = resources[plan_node->query_node.object.id];
      } else {
        object = runtime.createResource();
        if(plan_node->query_node.object.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.object.id) != 0) {
          resources[plan_node->query_node.object.id] = object;
        }
      }
      object->id = 0;
      break;
    case TripleOrder::OP:
    case TripleOrder::PO:
    case TripleOrder::POS:
      index_order = plan_node->triple_order;

      if(resources.count(plan_node->query_node.subject.id) != 0) {
        subject = resources[plan_node->query_node.subject.id];
      } else {
        subject = runtime.createResource();
        if(plan_node->query_node.subject.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.subject.id) != 0) {
          resources[plan_node->query_node.subject.id] = subject;
        }
      }
      subject->id = 0;

      if(resources.count(plan_node->query_node.predicate.id) != 0) {
        predicate = resources[plan_node->query_node.predicate.id];
      } else {
        predicate = runtime.createResource();
        if(plan_node->query_node.predicate.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.predicate.id) != 0) {
          resources[predicate->id] = predicate;
        }
      }
      predicate->id = plan_node->query_node.predicate.id;
      predicate->literal = plan_node->query_node.predicate.value;

      if(resources.count(plan_node->query_node.object.id) != 0) {
        object = resources[plan_node->query_node.object.id];
      } else {
        object = runtime.createResource();
        if(plan_node->query_node.object.type == QueryResource::Variable && plan_node->required_res.count(plan_node->query_node.object.id) != 0) {
          resources[plan_node->query_node.object.id] = object;
        }
      }
      object->id = plan_node->query_node.object.id;
      object->literal = plan_node->query_node.object.value;
      break;
  }

  Operator* opt = new TripleIndexLookup(runtime.db.getTripleIndex(plan_node->triple_order), plan_node->triple_order, plan_node->num_of_bounds, subject, predicate, object, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateSwitch(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  std::vector<Operator*> opts;
  for(int i=0; i<plan_node->child_nodes.size(); i++) {
    opts.push_back(generateInternal(plan_node->child_nodes[i], resources));
  }
  Operator* opt = new Switch(opts, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}

Operator* CodeGenerator::generateFilter(const PlanNode* plan_node, std::map<uint32_t, Resource*>& resources) {
  Operator* child = generateInternal(plan_node->child_nodes[0], resources);
  Operator* opt = nullptr;

  std::vector<Resource*> input_resources;
  input_resources.push_back(resources[plan_node->filter_key]);
  for(std::map<uint32_t, Resource*>::iterator iter = resources.begin(), end = resources.end(); iter != end; ++iter) {
    if(plan_node->filter_key != iter->first) {
      input_resources.push_back(iter->second);
    }
  }

  opt = new Filter(child, input_resources, plan_node->cardinality);
  runtime.moveToPool(opt);
  return opt;
}
