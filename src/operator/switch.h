#ifndef SWITCH_H
#define SWITCH_H

#include <limits.h>
#include "operator.h"


class Switch : public Operator {
public:
  Switch(const std::vector<Operator*>& inputs, double expected_cardinality);
  ~Switch();

  void open();
  void close();

  bool first();
  bool next();
  bool seek();

  double estimateCost();

protected:
  std::vector<Operator*> inputs;

  Operator* input;
};


#endif