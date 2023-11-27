#ifndef AGGREGATED_INDEX_H
#define AGGREGATED_INDEX_H

#include <string>
#include <vector>
#include <memory>
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/options.h"
#include "common/resource.h"

class AggregatedIndex {
private:
  #pragma pack(push, 1)
  struct Key {
    uint32_t x;
    uint32_t y;
  };
  struct Value {
    uint32_t count;
  };
  #pragma pack(pop)

  static const uint32_t PREFIX_SIZE = sizeof(uint32_t);
  static const uint32_t KEY_SIZE = sizeof(uint32_t) * 2;
  static const uint32_t VALUE_SIZE = sizeof(uint32_t);

public:
  AggregatedIndex(const std::string& store_path, const std::string& index_name);
  ~AggregatedIndex();

  bool open();
  bool close();

  bool get(Resource *res1, Resource *res2, int* count);
  bool get(Resource *res1, int* count);

  class Scanner {
  public:
    Scanner(AggregatedIndex& index, int num_of_keyres, Resource *res1, Resource *res2, std::vector<int>& counts);

    bool find();
    bool next();
  private:
    bool findByOneRes();
    bool nextByOneRes();
    bool findByTwoRes();
    bool nextByTwoRes();
    bool (AggregatedIndex::Scanner::*find_)();
    bool (AggregatedIndex::Scanner::*next_)();

    AggregatedIndex& index;

    Resource *res1, *res2;
    std::vector<int>& counts;

    rocksdb::Iterator* iter;
    char key_chunk[KEY_SIZE];
    Key* key ;
    char value_chunk[VALUE_SIZE];
    Value* value;

    std::vector<std::string> keys;
    std::vector<rocksdb::Status> statuses;
    std::vector<std::string> values;
    int idx;
    int res_idx;
  };
  friend class Scanner;
  class Writer {
  public:
    Writer(AggregatedIndex& index);
    ~Writer();

    bool put(const Resource& res1, const Resource& res2, int count);
    bool put(const Resource& res1, int count);
    bool flush();

  private:
    AggregatedIndex& index;

    rocksdb::WriteBatch* batch;
    char key_chunk[KEY_SIZE];
    Key* key;
    char value_chunk[VALUE_SIZE];
    Value* value;
  };
  friend class Writer;

private:
  bool read(const rocksdb::Slice& key, std::string* value);
  void read(const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values, std::vector<rocksdb::Status>* statuses);
  rocksdb::Iterator* getIterator();
  bool write(rocksdb::WriteBatch* batch);

  std::string store_path;
  std::string index_name;
  rocksdb::DB *db;
};


#endif