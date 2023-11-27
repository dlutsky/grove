#ifndef TRIPLE_INDEX_LOOKUP_H
#define TRIPLE_INDEX_LOOKUP_H

#include <memory>
#include <vector>
#include "operator.h"
#include "storage/triple_index.h"


class TripleIndexLookup : public Operator {
public:
  TripleIndexLookup(TripleIndex& index, TripleOrder triple_order, int num_of_bounds, Resource* subject, Resource* predicate, Resource* object, double expected_cardinality);
  ~TripleIndexLookup();

  void open();
  void close();

  bool first();
  bool next();
  bool seek();

  double estimateCost();

protected:
  TripleIndex& index;
  TripleOrder triple_order;
  int num_of_bounds;
  ResourcePosition ordering;
  Resource *subject, *predicate, *object;

  TripleIndex::Scanner* scanner;
};


#endif