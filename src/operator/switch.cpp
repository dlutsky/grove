#include "switch.h"


Switch::Switch(const std::vector<Operator*>& inputs, double expected_cardinality)
  : Operator(expected_cardinality), inputs(inputs) {
}

Switch::~Switch() {}

void Switch::open() {
  for(int i=0; i<inputs.size(); ++i) {
    inputs[i]->open();
  }
}

void Switch::close() {
  for(int i=0; i<inputs.size(); ++i) {
    inputs[i]->close();
  }
}

bool Switch::first() {
  double cost = std::numeric_limits<double>::max();
  for(int i=0; i<inputs.size(); ++i) {
    double c = inputs[i]->estimateCost();
    // std::cout<<"cost "<<i<<": "<<c<<std::endl;
    if(c < cost) {
      cost = c;
      this->input = inputs[i];
    }
  }
  return this->input->first();
}

bool Switch::next() {
  return this->input->next();
}

bool Switch::seek() {
  return false;
}

double Switch::estimateCost() {
  return expected_cardinality;
}