#include "aggregated_index.h"

AggregatedIndex::AggregatedIndex(const std::string& store_path, const std::string& index_name)
  : store_path(store_path), index_name(index_name), db(nullptr) {}

AggregatedIndex::~AggregatedIndex() {
  if(db != nullptr) {
    delete db;
  }
}

bool AggregatedIndex::open() {
  if(db != nullptr) {
    delete db;
  }
  rocksdb::Options options;
  options.create_if_missing = true;
  options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(PREFIX_SIZE));
  rocksdb::Status status = rocksdb::DB::Open(options, store_path + "/" + index_name, &db);
  return status.ok();
}

bool AggregatedIndex::close() {
  delete db;
  db = nullptr;
}

bool AggregatedIndex::get(Resource *res1, Resource *res2, int* count) {
  Key key = { res1->id, res2->id };
  std::string str;
  if(!read(std::string(reinterpret_cast<char*>(&key), KEY_SIZE), &str)){
    return false;
  }
  char chunk[VALUE_SIZE];
  Value* value = reinterpret_cast<Value*>(chunk);
  memcpy(chunk, str.c_str(), str.size());
  *count = value->count;
  return true;
}

bool AggregatedIndex::get(Resource *res1, int* count) {
  Key key = { 0, res1->id };
  std::string str;
  if(!read(std::string(reinterpret_cast<char*>(&key), KEY_SIZE), &str)){
    return false;
  }
  char chunk[VALUE_SIZE];
  Value* value = reinterpret_cast<Value*>(chunk);
  memcpy(chunk, str.c_str(), str.size());
  *count = value->count;
  return true;
}

AggregatedIndex::Scanner::Scanner(AggregatedIndex& index, int num_of_keyres, Resource *res1, Resource *res2, std::vector<int>& counts)
  : index(index), res1(res1), res2(res2), counts(counts) {
  key = reinterpret_cast<Key*>(key_chunk);
  value = reinterpret_cast<Value*>(value_chunk);
  if(num_of_keyres == 1) {
    find_ = &AggregatedIndex::Scanner::findByOneRes;
    next_ = &AggregatedIndex::Scanner::nextByOneRes;
  } else if(num_of_keyres == 2) {
    find_ = &AggregatedIndex::Scanner::findByTwoRes;
    next_ = &AggregatedIndex::Scanner::nextByTwoRes;
  }
}

bool AggregatedIndex::Scanner::find() {
  return (this->*find_)();
}

bool AggregatedIndex::Scanner::next() {
  return (this->*next_)();
}

bool AggregatedIndex::Scanner::findByOneRes() {
  iter = index.getIterator();
  iter->Seek(std::string(reinterpret_cast<char*>(&res1->id), PREFIX_SIZE));
  return iter->Valid();
}
bool AggregatedIndex::Scanner::nextByOneRes() {
  if(!iter->Valid()) {
    return false;
  }
  while (iter->Valid()) {
    memcpy(key_chunk, iter->key().data(), iter->key().size());
    memcpy(value_chunk, iter->value().data(), iter->value().size());
    res2->column.push_back(key->y);
    counts.push_back(value->count);
    iter->Next();
  }
  return true;
}

bool AggregatedIndex::Scanner::findByTwoRes() {
  std::vector<rocksdb::Slice> key_slices;
  key->x = res1->id;
  for(int i=0; i<res2->column.size(); i++) {
    key->y = res2->column[i];
    this->keys.push_back(std::string(key_chunk, KEY_SIZE));
    key_slices.push_back(rocksdb::Slice(this->keys[i]));
  }
  index.read(key_slices, &this->values, &this->statuses);
  if(this->values.empty()) {
    return false;
  }
  bool found = false;
  for(int i=0; i<this->statuses.size(); ++i) {
    if(this->statuses[i].ok()) {
      found = true;
      break;
    }
  }
  return found;
}
bool AggregatedIndex::Scanner::nextByTwoRes() {
  if(idx == statuses.size()) {
    return false;
  }
  while(idx < statuses.size()) {
    if(!this->statuses[idx].ok()) {
      ++idx;
      continue;
    }
    memcpy(value_chunk, values[idx].c_str(), values[idx].size());
    if(value->count > 0) {
      res2->column[res_idx] = res2->column[idx];
      ++res_idx;
      counts.push_back(value->count);
    }
    ++idx;
  }
  res2->column.resize(res_idx);
  return true;
}

AggregatedIndex::Writer::Writer(AggregatedIndex& index)
  : index(index), batch(nullptr) {
  batch = new rocksdb::WriteBatch();
  key = reinterpret_cast<Key*>(key_chunk);
  value = reinterpret_cast<Value*>(value_chunk);
}

AggregatedIndex::Writer::~Writer() {
  flush();
  delete batch;
}

bool AggregatedIndex::Writer::put(const Resource& res1, const Resource& res2, int count) {
  key->x = res1.id;
  key->y = res2.id;
  value->count = count;
  batch->Put(std::string(reinterpret_cast<char*>(key), KEY_SIZE), std::string(reinterpret_cast<char*>(value), VALUE_SIZE));
  if(batch->Count() > 1000) {
    flush();
  }
  return true;
}

bool AggregatedIndex::Writer::put(const Resource& res1, int count) {
  key->x = 0;
  key->y = res1.id;
  value->count = count;
  batch->Put(std::string(reinterpret_cast<char*>(key), KEY_SIZE), std::string(reinterpret_cast<char*>(value), VALUE_SIZE));
  if(batch->Count() > 1000) {
    flush();
  }
  return true;
}

bool AggregatedIndex::Writer::flush() {
  index.write(batch);
  batch->Clear();
  return true;
}

bool AggregatedIndex::read(const rocksdb::Slice& key, std::string* value) {
  rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, value);
  return status.ok();
}

void AggregatedIndex::read(const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values, std::vector<rocksdb::Status>* statuses) {
  *statuses = db->MultiGet(rocksdb::ReadOptions(), keys, values);
}

rocksdb::Iterator* AggregatedIndex::getIterator() {
  rocksdb::ReadOptions options = rocksdb::ReadOptions();
  options.prefix_same_as_start = true;
  rocksdb::Iterator* iter = db->NewIterator(options);
  return iter;
}

bool AggregatedIndex::write(rocksdb::WriteBatch* batch) {
  if(batch->Count() == 0) {
    return true;
  }
  rocksdb::Status status = db->Write(rocksdb::WriteOptions(), batch);
  return status.ok();
}