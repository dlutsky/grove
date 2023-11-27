#ifndef FILTER_H
#define FILTER_H

#include <vector>
#include "operator.h"
#include "util/ribbon_filter.h"

class Filter : public Operator {
public:
  Filter(Operator* input, const std::vector<Resource*>& input_resources, double expected_cardinality);
  ~Filter();

  void open();
  void close();

  bool first();
  bool next();
  bool seek();

  double estimateCost();

protected:
  Operator* input;
  std::vector<Resource*> input_resources;

  RibbonFilter<uint32_t>* ribbon_filter;

  int idx;
};


#endif
