#include "filter.h"

Filter::Filter(Operator* input, const std::vector<Resource*>& input_resources, double expected_cardinality)
  : Operator(expected_cardinality), input(input), input_resources(input_resources), ribbon_filter(nullptr), idx(0) {}

Filter::~Filter() {
  if(ribbon_filter != nullptr) {
    delete ribbon_filter;
  }
}

void Filter::open() {
  this->input->open();
}

void Filter::close() {
  this->input->close();
}

bool Filter::first() {
  if(this->input_resources.empty()) {
    return false;
  }

  if(this->input_resources[0]->column.empty()) {
    return false;
  }

  this->ribbon_filter = new RibbonFilter<uint32_t>(this->input_resources[0]->column.size());
  this->ribbon_filter->addAll(this->input_resources[0]->column);
  this->input_resources[0]->column.clear();

  if(!this->input->first()) {
    return false;
  }
  int k=idx;
  for(int i=idx; i<this->input_resources[0]->column.size(); ++i) {
    if(this->ribbon_filter->contain(this->input_resources[0]->column[i])) {
      for(int j=0; j<this->input_resources.size(); ++j) {
        this->input_resources[j]->column[k] = this->input_resources[j]->column[i];
      }
      ++k;
    }
  }
  if(k == idx) {
    while(this->input->next()) {
      for(int i=idx; i<this->input_resources[0]->column.size(); ++i) {
        if(this->ribbon_filter->contain(this->input_resources[0]->column[i])) {
          for(int j=0; j<this->input_resources.size(); ++j) {
            this->input_resources[j]->column[k] = this->input_resources[j]->column[i];
          }
          ++k;
        }
      }
      if(k != idx) {
        idx = k;
        for(int j=0; j<this->input_resources.size(); ++j) {
          this->input_resources[j]->column.resize(idx);
        }
        return true;
      }
    }
    return false;
  }
  idx = k;
  for(int j=0; j<this->input_resources.size(); ++j) {
    this->input_resources[j]->column.resize(idx);
  }
  return true;
}

bool Filter::next() {
  int k=idx;
  while(this->input->next()) {
    for(int i=idx; i<this->input_resources[0]->column.size(); ++i) {
      if(this->ribbon_filter->contain(this->input_resources[0]->column[i])) {
        for(int j=0; j<this->input_resources.size(); ++j) {
          this->input_resources[j]->column[k] = this->input_resources[j]->column[i];
        }
        ++k;
      }
    }
    if(k != idx) {
      idx = k;
      for(int j=0; j<this->input_resources.size(); ++j) {
        this->input_resources[j]->column.resize(idx);
      }
      return true;
    }
  }
  return false;
}

bool Filter::seek() {
  return false;
}

double Filter::estimateCost() {
  return input->estimateCost();
}
