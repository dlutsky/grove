#include "triple_index.h"
#include <iostream>

TripleIndex::TripleIndex(const std::string& store_path, const std::string& index_name)
  : store_path(store_path), index_name(index_name), db(nullptr), comparator() {}

TripleIndex::~TripleIndex() {
  if(db != nullptr) {
    delete db;
  }
}

bool TripleIndex::open() {
  if(db != nullptr) {
    delete db;
  }
  rocksdb::Options options;
  options.create_if_missing = true;
  options.comparator = &comparator;
  // options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(TWO_PREFIX_SIZE));
  rocksdb::Status status = rocksdb::DB::Open(options, store_path + "/" + index_name, &db);
  return status.ok();
}

bool TripleIndex::close() {
  if(db != nullptr) {
    delete db;
  }
  db = nullptr;
  return true;
}

TripleIndex::Scanner::Scanner(TripleIndex& index, int num_of_keyres, int num_of_bounds, Resource *res1, Resource *res2, Resource *res3)
  : index(index), num_of_keyres(num_of_keyres), num_of_bounds(num_of_bounds), res1(res1), res2(res2), res3(res3), iter(nullptr), idx(0) {
  search_key = reinterpret_cast<Key*>(search_key_chunk);
  search_bound = reinterpret_cast<Key*>(search_bound_chunk);
  if(num_of_keyres == 1) {
    find_ = &TripleIndex::Scanner::findByOneRes;
    seek_ = &TripleIndex::Scanner::seekByOneRes;
    next_ = &TripleIndex::Scanner::nextByOneRes;
  } else if(num_of_keyres == 2) {
    find_ = &TripleIndex::Scanner::findByTwoRes;
    seek_ = &TripleIndex::Scanner::seekByTwoRes;
    next_ = &TripleIndex::Scanner::nextByTwoRes;
  } else if(num_of_keyres == 3) {
    find_ = &TripleIndex::Scanner::findByThreeRes;
    seek_ = &TripleIndex::Scanner::seekByThreeRes;
    next_ = &TripleIndex::Scanner::nextByThreeRes;
  }
}

TripleIndex::Scanner::~Scanner() {
  if(this->iter != nullptr) {
    delete this->iter;
  }
}

bool TripleIndex::Scanner::find() {
  return (this->*find_)();
}

bool TripleIndex::Scanner::seek() {
  return (this->*seek_)();
}

bool TripleIndex::Scanner::next() {
  return (this->*next_)();
}

bool TripleIndex::Scanner::findByOneRes() {
  if(this->iter != nullptr) {
    delete this->iter;
    this->iter = nullptr;
  }

  idx = 0;
  search_bound->x = res1->id;
  search_bound->y = UINT32_MAX;
  search_bound->z = UINT32_MAX;
  this->upper_bound = rocksdb::Slice(search_bound_chunk, KEY_SIZE);
  this->iter = index.getIterator(&this->upper_bound);
  search_key->x = res1->id;
  search_key->y = 0;
  search_key->z = 0;
  this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
  if(!this->iter->Valid()) {
    return false;
  }
  result_key = reinterpret_cast<const Key*>(iter->key().data());
  res2->column.push_back(result_key->y);
  res3->column.push_back(result_key->z);
  return true;
}

bool TripleIndex::Scanner::seekByOneRes() {
  return false;
}

bool TripleIndex::Scanner::nextByOneRes() {
  if(idx==1) {
    return false;
  }
  this->iter->Next();
  while (this->iter->Valid()) {
    result_key = reinterpret_cast<const Key*>(iter->key().data());
    res2->column.push_back(result_key->y);
    res3->column.push_back(result_key->z);
    this->iter->Next();
  }
  idx = 1;
  return true;
}

bool TripleIndex::Scanner::findByTwoRes() {
  if(this->iter != nullptr) {
    delete this->iter;
    this->iter = nullptr;
  }

  idx = 0;
  res3->column.clear();
  if(!res2->column.empty()) {
    this->res_column = res2->column;
    res2->column.clear();
    search_bound->x = res1->id;
    search_bound->y = UINT32_MAX;
    search_bound->z = UINT32_MAX;
    this->upper_bound = rocksdb::Slice(search_bound_chunk, KEY_SIZE);
    this->iter = index.getIterator(&this->upper_bound);
    search_key->x = res1->id;
    search_key->z = 0;
    while(idx < this->res_column.size()) {
      search_key->y = this->res_column[idx];
      this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
      if(this->iter->Valid()) {
        result_key = reinterpret_cast<const Key*>(this->iter->key().data());
        if(result_key->y == search_key->y) {
          res2->column.push_back(result_key->y);
          res3->column.push_back(result_key->z);
          return true;
        }
      }
      ++idx;
    }
    return false;
  } else {
    this->res_column.clear();
    this->res_column.push_back(res2->id);
    search_bound->x = res1->id;
    if(this->num_of_bounds == 1) {
      search_bound->y = UINT32_MAX;
    } else {
      search_bound->y = this->res_column[idx];
    }
    search_bound->z = UINT32_MAX;
    this->upper_bound = rocksdb::Slice(search_bound_chunk, KEY_SIZE);
    this->iter = index.getIterator(&this->upper_bound);
    search_key->x = res1->id;
    search_key->y = this->res_column[idx];
    search_key->z = 0;
    this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
    if(!this->iter->Valid()) {
      return false;
    }
    result_key = reinterpret_cast<const Key*>(this->iter->key().data());
    res2->column.push_back(result_key->y);
    res3->column.push_back(result_key->z);
    // res3->id = result_key->z + 1;
    return true;
  }
}

bool TripleIndex::Scanner::seekByTwoRes() {
  res2->column.clear();
  res3->column.clear();
  
  search_key->x = res1->id;
  search_key->y = res2->id;
  search_key->z = 0;
  this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));

  bool found = false;
  res2->id = UINT32_MAX;
  while(this->iter->Valid()) {
    result_key = reinterpret_cast<const Key*>(iter->key().data());
    if(result_key->y != search_key->y) {
      res2->id = result_key->y;
      break;
    }
    res2->column.push_back(result_key->y);
    res3->column.push_back(result_key->z);
    this->iter->Next();
    found = true;
  }
  return found;
}

bool TripleIndex::Scanner::nextByTwoRes() {
  if(idx == this->res_column.size()) {
    return false;
  }
  this->iter->Next();
  while(true) {
    while(this->iter->Valid()) {
      result_key = reinterpret_cast<const Key*>(this->iter->key().data());
      if(result_key->y != search_key->y) {
        break;
      }
      res2->column.push_back(result_key->y);
      res3->column.push_back(result_key->z);
      this->iter->Next();
    }
    
    ++idx;

    if(idx >= this->res_column.size()) {
      break;
    }

    search_key->y = this->res_column[idx];
    this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
  }
  return true;
}

bool TripleIndex::Scanner::findByThreeRes() {
  if(this->iter != nullptr) {
    delete this->iter;
    this->iter = nullptr;
  }

  idx = 0;
  res2->column.clear();

  this->res_column = res3->column;
  res3->column.clear();
  if(this->res_column.empty()) {
    this->res_column.push_back(res3->id);
  }
  
  search_bound->x = res1->id;
  if(this->num_of_bounds == 1) {
    search_bound->y = UINT32_MAX;
  } else {
    search_bound->y = res2->id;
  }
  search_bound->z = UINT32_MAX;
  this->upper_bound = rocksdb::Slice(search_bound_chunk, KEY_SIZE);
  this->iter = index.getIterator(&this->upper_bound);
  search_key->x = res1->id;
  search_key->y = res2->id;
  while(idx < this->res_column.size()) {
    search_key->z = this->res_column[idx];
    this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
    if(this->iter->Valid()) {
      result_key = reinterpret_cast<const Key*>(this->iter->key().data());
      if(result_key->z == search_key->z) {
        res2->column.push_back(result_key->y);
        res3->column.push_back(result_key->z);
        return true;
      }
    }
    ++idx;
  }
  return false;
}

bool TripleIndex::Scanner::seekByThreeRes() {
  res2->column.clear();
  res3->column.clear();

  search_key->x = res1->id;
  search_key->y = res2->id;
  search_key->z = res3->id;
  this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
  
  bool found = false;
  res3->id = UINT32_MAX;
  if(this->num_of_bounds == 1) {
    while(this->iter->Valid()) {
      result_key = reinterpret_cast<const Key*>(iter->key().data());
      if(result_key->z != search_key->z || result_key->y != search_key->y) {
        res3->id = result_key->z;
        break;
      }
      res2->column.push_back(result_key->y);
      res3->column.push_back(result_key->z);
      this->iter->Next();
      found = true;
    }
  } else {
    while(this->iter->Valid()) {
      result_key = reinterpret_cast<const Key*>(iter->key().data());
      if(result_key->z != search_key->z) {
        res3->id = result_key->z;
        break;
      }
      res2->column.push_back(result_key->y);
      res3->column.push_back(result_key->z);
      this->iter->Next();
      found = true;
    }
  }
  return found;
}

bool TripleIndex::Scanner::nextByThreeRes() {
  if(idx == this->res_column.size()) {
    return false;
  }
  this->iter->Next();
  while(true) {
    while(this->iter->Valid()) {
      result_key = reinterpret_cast<const Key*>(this->iter->key().data());
      if(result_key->z != search_key->z) {
        break;
      }
      res2->column.push_back(result_key->y);
      res3->column.push_back(result_key->z);
      this->iter->Next();
    }
    
    ++idx;

    if(idx >= this->res_column.size()) {
      break;
    }

    search_key->z = this->res_column[idx];
    this->iter->Seek(rocksdb::Slice(search_key_chunk, KEY_SIZE));
  }
  return true;
}

TripleIndex::Writer::Writer(TripleIndex& index)
  : index(index) {
  batch = new rocksdb::WriteBatch();
  key = reinterpret_cast<Key*>(key_chunk);
}

TripleIndex::Writer::~Writer() {
  flush();
  delete batch;
}

bool TripleIndex::Writer::put(const Resource& res1, const Resource& res2, const Resource& res3) {
  if(!res3.column.empty()) {
    key->x = res1.id;
    key->y = res2.id;
    for(int i=0; i<res3.column.size(); i++) {
      key->z = res3.column[i];
      batch->Put(std::string(key_chunk, KEY_SIZE), "");
    }
  } else {
    key->x = res1.id;
    key->y = res2.id;
    key->z = res3.id;
    batch->Put(std::string(key_chunk, KEY_SIZE), "");
  }

  if(batch->Count() > 1000) {
    flush();
  }

  return true;
}

bool TripleIndex::Writer::flush() {
  index.write(batch);
  batch->Clear();
  return true;
}

bool TripleIndex::read(const rocksdb::Slice& key, std::string* value) {
  rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, value);
  return status.ok();
}

void TripleIndex::read(const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values, std::vector<rocksdb::Status>* statuses) {
  *statuses = db->MultiGet(rocksdb::ReadOptions(), keys, values);
}

rocksdb::Iterator* TripleIndex::getIterator() {
  rocksdb::ReadOptions options = rocksdb::ReadOptions();
  // options.prefix_same_as_start = true;
  rocksdb::Iterator* iter = db->NewIterator(options);
  return iter;
}

rocksdb::Iterator* TripleIndex::getIterator(rocksdb::Slice* upper_bound) {
  rocksdb::ReadOptions options = rocksdb::ReadOptions();
  options.iterate_upper_bound = upper_bound;
  // options.total_order_seek = true;
  rocksdb::Iterator* iter = db->NewIterator(options);
  return iter;
}

bool TripleIndex::write(rocksdb::WriteBatch* batch) {
  if(batch->Count() == 0) {
    return true;
  }
  rocksdb::Status status = db->Write(rocksdb::WriteOptions(), batch);
  return status.ok();
}

TripleIndex::TripleComparator::TripleComparator() {}

int TripleIndex::TripleComparator::Compare(const rocksdb::Slice& a, const rocksdb::Slice& b) const {
  const key_t* key1 = reinterpret_cast<const key_t*>(a.data());
  const key_t* key2 = reinterpret_cast<const key_t*>(b.data());
  if(key1->xy != key2->xy) {
    int64_t r = int64_t(key1->x) - int64_t(key2->x);
    if (r == 0) {
      r = int64_t(key1->y) - int64_t(key2->y);
    }
    return r%INT32_MAX;
  } else {
    return (int64_t(key1->z) - int64_t(key2->z))%INT32_MAX;
  }
}

bool TripleIndex::TripleComparator::Equal(const rocksdb::Slice& a, const rocksdb::Slice& b) const {
  const Key* key1 = reinterpret_cast<const Key*>(a.data());
  const Key* key2 = reinterpret_cast<const Key*>(b.data());
  return key1->x == key2->x && key1->y == key2->y && key1->z == key2->z;
  // return a == b;
}

const char* TripleIndex::TripleComparator::Name() const {
  return "grove.TripleComparator";
}

void TripleIndex::TripleComparator::FindShortestSeparator(std::string* start, const rocksdb::Slice& limit) const {
}

void TripleIndex::TripleComparator::FindShortSuccessor(std::string* key) const {
}

bool TripleIndex::TripleComparator::IsSameLengthImmediateSuccessor(const rocksdb::Slice& s, const rocksdb::Slice& t) const {
  const Key* key1 = reinterpret_cast<const Key*>(s.data());
  const Key* key2 = reinterpret_cast<const Key*>(t.data());
  if(key1->x == key2->x && key1->y == key2->y && key1->z + 1 == key2->z) {
    return true;
  } else {
    return false;
  }
}

bool TripleIndex::TripleComparator::CanKeysWithDifferentByteContentsBeEqual() const {
  return false;
}

int TripleIndex::TripleComparator::CompareWithoutTimestamp(const rocksdb::Slice& a, bool a_has_ts, const rocksdb::Slice& b, bool b_has_ts) const {
  const key_t* key1 = reinterpret_cast<const key_t*>(a.data());
  const key_t* key2 = reinterpret_cast<const key_t*>(b.data());
  if(key1->xy != key2->xy) {
    int64_t r = int64_t(key1->x) - int64_t(key2->x);
    if (r == 0) {
      r = int64_t(key1->y) - int64_t(key2->y);
    }
    return r%INT32_MAX;
  } else {
    return (int64_t(key1->z) - int64_t(key2->z))%INT32_MAX;
  }
}

bool TripleIndex::TripleComparator::EqualWithoutTimestamp(const rocksdb::Slice& a, const rocksdb::Slice& b) const {
  const Key* key1 = reinterpret_cast<const Key*>(a.data());
  const Key* key2 = reinterpret_cast<const Key*>(b.data());
  return key1->x == key2->x && key1->y == key2->y && key1->z == key2->z;
  //return a == b;
}