#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <memory>
#include "statistics_manager.h"
#include "buffer/buffer_manager.h"
#include "storage/dictionary.h"
#include "storage/triple_table.h"
#include "storage/triple_index.h"
#include "storage/aggregated_index.h"
#include "query/query_graph.h"



class Database {
public:
  explicit Database(const std::string& db_name);

  static bool create(const std::string& db_name);
  bool open();
  void close();

  void executeQuery(QueryGraph* query_graph, bool explain=false, bool silent=false);
  void executeQuery(QueryGraph* query_graph, const std::string& out_file_path, bool explain=false, bool silent=false);

  Dictionary& getDictionary();
  TripleTable& getTripleTable();

  TripleIndex& getTripleIndex(TripleOrder triple_order);
  AggregatedIndex& getAggregatedIndex(TripleOrder triple_order);

private:
  std::string store_path;
  std::string db_name;

  std::unique_ptr<Dictionary> dict;
  std::unique_ptr<TripleTable> triple_table;

  // std::unique_ptr<WiredTigerConnection> conn;

  std::unique_ptr<TripleIndex> pso_index;
  std::unique_ptr<TripleIndex> pos_index;
  std::unique_ptr<AggregatedIndex> ps_index;
  std::unique_ptr<AggregatedIndex> po_index;

  BufferManager buffer_manager;
};





#endif
