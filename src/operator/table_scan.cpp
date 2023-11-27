#include "table_scan.h"
#include <iostream>

TableScan::TableScan(TripleTable& table, TripleOrder triple_order, Resource* subject, Resource* predicate, Resource* object, double expected_cardinality)
  : Operator(expected_cardinality), table(table), triple_order(triple_order), subject(subject), predicate(predicate), object(object), scanner(nullptr) {}

TableScan::~TableScan() {
  if(scanner != nullptr) {
    delete scanner;
  }
}

void TableScan::open() {
  this->subject->column.reserve(this->expected_cardinality+10);
  this->object->column.reserve(this->expected_cardinality+10);
}

void TableScan::close() {}

bool TableScan::first() {
  this->subject->column.clear();
  this->object->column.clear();
  scanner = new TripleTable::Scanner(this->table, predicate, subject, object);
  if(!scanner->find()) {
    return false;
  }
  return scanner->next();
}

bool TableScan::next() {
  return scanner->next();
}

bool TableScan::seek() {
  return false;
}

double TableScan::estimateCost() {
  return expected_cardinality;
}