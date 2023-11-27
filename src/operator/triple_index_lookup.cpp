#include "triple_index_lookup.h"
#include <iostream>


TripleIndexLookup::TripleIndexLookup(TripleIndex& index, TripleOrder triple_order, int num_of_bounds, Resource* subject, Resource* predicate, Resource* object, double expected_cardinality)
  : Operator(expected_cardinality), index(index), triple_order(triple_order), num_of_bounds(num_of_bounds), subject(subject), predicate(predicate), object(object), scanner(nullptr) {}

TripleIndexLookup::~TripleIndexLookup() {
  if(scanner != nullptr) {
    delete scanner;
  }
}

void TripleIndexLookup::open() {
  this->subject->column.reserve(this->expected_cardinality+10);
  this->object->column.reserve(this->expected_cardinality+10);
}

void TripleIndexLookup::close() {}

bool TripleIndexLookup::first() {
  switch(triple_order) {
    case TripleOrder::P:
      subject->column.clear();
      object->column.clear();
      scanner = new TripleIndex::Scanner(this->index, 1, num_of_bounds, predicate, subject, object);
      break;
    case TripleOrder::SP:
    case TripleOrder::PS:
      object->column.clear();
      scanner = new TripleIndex::Scanner(this->index, 2, num_of_bounds, predicate, subject, object);
      break;
    case TripleOrder::OP:
    case TripleOrder::PO:
      subject->column.clear();
      scanner = new TripleIndex::Scanner(this->index, 2, num_of_bounds, predicate, object, subject);
      break;
    case TripleOrder::PSO:
      scanner = new TripleIndex::Scanner(this->index, 3, num_of_bounds, predicate, subject, object);
      break;
    case TripleOrder::POS:
      scanner = new TripleIndex::Scanner(this->index, 3, num_of_bounds, predicate, object, subject);
      break;
  }
  if(scanner == nullptr) {
    return false;
  }
  return scanner->find();
}

bool TripleIndexLookup::next() {
  return scanner->next();
}

bool TripleIndexLookup::seek() {
  // return scanner->find();
  return scanner->seek();
}

double TripleIndexLookup::estimateCost() {
  switch(triple_order) {
    case TripleOrder::P:
      return expected_cardinality;
    case TripleOrder::SP:
    case TripleOrder::PS:
      if(subject->column.empty()) {
        return expected_cardinality;
      } else {
        return subject->column.size()*20;
      }
    case TripleOrder::OP:
    case TripleOrder::PO:
      if(object->column.empty()) {
        return expected_cardinality;
      } else {
        return object->column.size()*20;
      }
    case TripleOrder::PSO:
      return object->column.size()*20;
    case TripleOrder::POS:
      return subject->column.size()*20;
  }
  return expected_cardinality;
}