#ifndef OPERATOR_H
#define OPERATOR_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include "common/resource.h"
#include "common/triple.h"
#include <iostream>


class Operator {
public:
  virtual ~Operator();

  virtual void open() = 0;
  virtual void close() = 0;

  //void consume(int idx);
  //void produce(Operator* parent, int idx);

  //virtual bool first(std::vector<int>& counts) = 0;
  //virtual bool next(std::vector<int>& counts) = 0;

  virtual bool first() = 0;
  virtual bool next() = 0;
  virtual bool seek() = 0;

  double getExpectedCardinality();
  virtual double estimateCost() = 0;

protected:
  Operator(double expected_cardinality);

  double expected_cardinality;
};



#endif
