#include "operator.h"


Operator::Operator(double expected_cardinality) : expected_cardinality(expected_cardinality) {}

Operator::~Operator() {}

double Operator::getExpectedCardinality() {
  return expected_cardinality;
}
