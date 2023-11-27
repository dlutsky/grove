#include "database.h"
#include "config.h"
#include "util/file_directory.h"
#include "query/semantic_analyzer.h"
#include "plan/query_plan.h"
#include "plan/query_planner.h"
#include "runtime/runtime.h"
#include <iostream>
#include <chrono>
#include <ratio>


Database::Database(const std::string& db_name) : buffer_manager(10000) {
  Config config;
  store_path = config.getParam(ConfigKey::STORE_PATH) + "/" + db_name;
}

bool Database::create(const std::string& db_name) {
  Config config;
  std::string store_path = config.getParam(ConfigKey::STORE_PATH) + "/" + db_name;
  if(Directory::exist(store_path)) {
    return false;
  }
  Directory::create(store_path);

  return true;
}

bool Database::open() {
  /*
  conn = std::unique_ptr<WiredTigerConnection>(new WiredTigerConnection(store_path));
  if(!conn->open()) {
    return false;
  }
  */
  dict = std::unique_ptr<Dictionary>(new Dictionary(store_path));
  if(!dict->open()) {
    return false;
  }
  triple_table = std::unique_ptr<TripleTable>(new TripleTable(store_path, "triple_table", &buffer_manager));
  if(!triple_table->open()) {
    return false;
  }
  pso_index = std::make_unique<TripleIndex>(store_path, "pso_index");
  if(!pso_index->open()) {
    return false;
  }
  pos_index = std::make_unique<TripleIndex>(store_path, "pos_index");
  if(!pos_index->open()) {
    return false;
  }
  ps_index = std::make_unique<AggregatedIndex>(store_path, "ps_index");
  if(!ps_index->open()) {
    return false;
  }
  po_index = std::make_unique<AggregatedIndex>(store_path, "po_index");
  if(!po_index->open()) {
    return false;
  }
}

void Database::close() {
  dict->close();
  triple_table->close();
  pso_index->close();
  pos_index->close();
  ps_index->close();
  po_index->close();
  // conn->close();
}

void Database::executeQuery(QueryGraph* query_graph, bool explain, bool silent) {
  SemanticAnalyzer::analyse(query_graph, *dict);
  StatisticsManager stat_manager(*dict, *triple_table, getAggregatedIndex(TripleOrder::SP), getAggregatedIndex(TripleOrder::OP));
  std::unique_ptr<QueryPlan> query_plan = QueryPlanner::build(*query_graph, stat_manager);
  if(explain){
    query_plan->print();
  }

  Runtime runtime(*this);
  //auto start = std::chrono::high_resolution_clock::now();
  query_plan->execute(runtime, silent);
  //auto done = std::chrono::high_resolution_clock::now();
  //double exec_time = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(done-start).count();
  //std::cout << std::endl << "Running time: " << exec_time << " ms" << std::endl;
}

void Database::executeQuery(QueryGraph* query_graph, const std::string& out_file_path, bool explain, bool silent) {
  SemanticAnalyzer::analyse(query_graph, *dict);
  StatisticsManager stat_manager(*dict, *triple_table, getAggregatedIndex(TripleOrder::SP), (getAggregatedIndex(TripleOrder::OP)));
  std::unique_ptr<QueryPlan> query_plan = QueryPlanner::build(*query_graph, stat_manager);
  Runtime runtime(*this, out_file_path);
  query_plan->execute(runtime, silent);
}

Dictionary& Database::getDictionary() {
  return *dict;
}

TripleTable& Database::getTripleTable() {
  return *triple_table;
}

TripleIndex& Database::getTripleIndex(TripleOrder triple_order) {
  switch(triple_order) {
    case TripleOrder::SP:
    case TripleOrder::PS:
    case TripleOrder::PSO:
      return *pso_index;
    case TripleOrder::OP:
    case TripleOrder::PO:
    case TripleOrder::POS:
      return *pos_index;
    default:
      return *pso_index;
  }
}

AggregatedIndex& Database::getAggregatedIndex(TripleOrder triple_order) {
  switch(triple_order) {
    case TripleOrder::SP:
      return *ps_index;
    case TripleOrder::OP:
      return *po_index;
    default:
      return *ps_index;
  }
}
