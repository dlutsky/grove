#ifndef TRIPLE_INDEX_H
#define TRIPLE_INDEX_H

#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <cstdint>
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/options.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/comparator.h"
#include "common/resource.h"


class TripleIndex {
private:
  #pragma pack(push, 1)
  struct Key {
    uint32_t x;
    uint32_t y;
    uint32_t z;
  };
  struct key_t {
    union {
      uint64_t xy;
      struct {
        uint32_t x;
        uint32_t y;
      };
    };
    uint32_t z;
  };
  #pragma pack(pop)

public:
  TripleIndex(const std::string& store_path, const std::string& index_name);
  ~TripleIndex();

  bool open();
  bool close();

  static const uint32_t ONE_PREFIX_SIZE = sizeof(uint32_t);
  static const uint32_t TWO_PREFIX_SIZE = sizeof(uint32_t) * 2;
  static const uint32_t KEY_SIZE = sizeof(uint32_t) * 3;

  class Scanner {
  public:
    Scanner(TripleIndex& index, int num_of_keyres, int num_of_bounds, Resource *res1, Resource *res2, Resource *res3);
    ~Scanner();

    bool find();
    bool seek();
    bool next();
  private:
    bool findByOneRes();
    bool seekByOneRes();
    bool nextByOneRes();
    bool findByTwoRes();
    bool seekByTwoRes();
    bool nextByTwoRes();
    bool findByThreeRes();
    bool seekByThreeRes();
    bool nextByThreeRes();
    bool (TripleIndex::Scanner::*find_)();
    bool (TripleIndex::Scanner::*seek_)();
    bool (TripleIndex::Scanner::*next_)();

    TripleIndex& index;

    Resource *res1, *res2, *res3;
    int num_of_keyres;
    int num_of_bounds;

    rocksdb::Iterator* iter;
    rocksdb::Slice upper_bound;
    std::vector<uint32_t> res_column;
    //std::vector<std::string> keys;
    //std::vector<rocksdb::Status> statuses;
    char search_bound_chunk[KEY_SIZE];
    Key* search_bound;
    char search_key_chunk[KEY_SIZE];
    Key* search_key;
    const Key* result_key;
    int idx;
  };
  friend class Scanner;
  class Writer {
  public:
    Writer(TripleIndex& index);
    ~Writer();

    bool put(const Resource& res1, const Resource& res2, const Resource& res3);
    bool flush();

  private:
    TripleIndex& index;

    rocksdb::WriteBatch* batch;
    char key_chunk[KEY_SIZE];
    Key* key;
  };
  friend class Writer;

  class TripleComparator : public rocksdb::Comparator {
  public:
    TripleComparator();
    int Compare(const rocksdb::Slice& a, const rocksdb::Slice& b) const override;
    bool Equal(const rocksdb::Slice& a, const rocksdb::Slice& b) const override;
    const char* Name() const override;
    void FindShortestSeparator(std::string* start, const rocksdb::Slice& limit) const override;
    void FindShortSuccessor(std::string* key) const override;
    bool IsSameLengthImmediateSuccessor(const rocksdb::Slice& s, const rocksdb::Slice& t) const override;
    bool CanKeysWithDifferentByteContentsBeEqual() const override;
    int CompareWithoutTimestamp(const rocksdb::Slice& a, bool a_has_ts, const rocksdb::Slice& b, bool b_has_ts) const override;
    bool EqualWithoutTimestamp(const rocksdb::Slice& a, const rocksdb::Slice& b) const override;
  };

private:
  bool read(const rocksdb::Slice& key, std::string* value);
  void read(const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values, std::vector<rocksdb::Status>* statuses);
  rocksdb::Iterator* getIterator();
  rocksdb::Iterator* getIterator(rocksdb::Slice* upper_bound);
  bool write(rocksdb::WriteBatch* batch);

  std::string store_path;
  std::string index_name;
  rocksdb::DB *db;

  TripleComparator comparator;
};


#endif