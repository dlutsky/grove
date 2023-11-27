#ifndef STATISTICS_MANAGER_H
#define STATISTICS_MANAGER_H

#include <cstdint>
#include <sys/types.h>
#include "storage/dictionary.h"
#include "storage/triple_table.h"
#include "storage/aggregated_index.h"


class StatisticsManager {
public:
  StatisticsManager(Dictionary& dict, TripleTable& triple_table, AggregatedIndex& ps_aggregated_index, AggregatedIndex& po_aggregated_index);
  ~StatisticsManager();

  uint32_t getCount(uint32_t subject, uint32_t predicate, uint32_t object);
  uint32_t getCount(TripleOrder key_order, uint32_t subject, uint32_t predicate, uint32_t object);
  uint32_t getDistinctCount(TripleOrder key_order, uint32_t subject, uint32_t predicate, uint32_t object);

private:
  Dictionary& dict;
  TripleTable& triple_table;
  AggregatedIndex& ps_aggregated_index;
  AggregatedIndex& po_aggregated_index;
};




#endif
