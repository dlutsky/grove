#include "statistics_manager.h"

StatisticsManager::StatisticsManager(Dictionary& dict, TripleTable& triple_table, AggregatedIndex& ps_aggregated_index, AggregatedIndex& po_aggregated_index)
  : dict(dict), triple_table(triple_table), ps_aggregated_index(ps_aggregated_index), po_aggregated_index(po_aggregated_index) {
}

StatisticsManager::~StatisticsManager() {}

uint32_t StatisticsManager::getCount(uint32_t subject, uint32_t predicate, uint32_t object) {
  Resource res1, res2;
  int count;
  if(subject) {
    if(predicate) {
      if(object) {
        return 1;
      }
      else {
        // sp
        res1.id = predicate;
        res2.id = subject;
        this->ps_aggregated_index.get(&res1, &res2, &count);
        return count;
      }
    }
    else {
      if(object) {
        // so
        return 1;
      }
      else {
        // s
        // todo
        return 1;
      }
    }
  }
  else {
    if(predicate) {
      if(object) {
        // op
        res1.id = predicate;
        res2.id = object;
        this->po_aggregated_index.get(&res1, &res2, &count);
        return count;
      }
      else {
        // p
        return this->triple_table.count(predicate);
      }
    }
    else {
      if(object) {
        // o
        // todo
        return 1;
      }
      else {
        return 0;
      }
    }
  }
}

uint32_t StatisticsManager::getCount(TripleOrder key_order, uint32_t subject, uint32_t predicate, uint32_t object) {
  Resource res1, res2;
  int count = 0;
  switch(key_order) {
    case P:
      return this->triple_table.count(predicate);
    case SP:
    case PS:
      res1.id = predicate;
      res2.id = subject;
      this->ps_aggregated_index.get(&res1, &res2, &count);
      return count;
    case OP:
    case PO:
      res1.id = predicate;
      res2.id = object;
      this->po_aggregated_index.get(&res1, &res2, &count);
      return count;
  }
  return 1;
}

uint32_t StatisticsManager::getDistinctCount(TripleOrder key_order, uint32_t subject, uint32_t predicate, uint32_t object) {
  Resource res1;
  int count = 1;
  res1.id = predicate;
  switch(key_order) {
    case SP:
    case PS:
      this->ps_aggregated_index.get(&res1, &count);
      return count;
    case OP:
    case PO:
      this->po_aggregated_index.get(&res1, &count);
      return count;
  }
  return 1;
}
