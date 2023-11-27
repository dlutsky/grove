#ifndef TRIPLE_TABLE_H
#define TRIPLE_TABLE_H

#include <string>
#include "common/triple.h"
#include "common/resource.h"
#include "common/constants.h"
#include "buffer/buffer_manager.h"
#include "bdb_file.h"
#include "util/file_directory.h"
#include "thread/mutex.h"
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/options.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/sst_file_writer.h"


// Vertically partitioned triple table

class TripleTable {
private:
  #pragma pack(push, 1)
  // KVStore
  struct Key {
    uint32_t x;
  };
  struct Segment {
    uint32_t count;
    uint32_t first_block_no;
    uint32_t last_block_no;
    uint16_t dtype;
    uint16_t dsize;
    char data[1];
  };
  struct Node {
    uint32_t block_no;
    uint32_t next_block_no;
    uint16_t dtype;
    uint16_t dsize;
    char data[1];
  };
  struct Entry {
    uint32_t x;
    uint32_t y;
  };
  #pragma pack(pop)

  static const uint32_t KEY_SIZE = sizeof(uint32_t);

  static const uint32_t SEGMENT_MAX_SIZE = 4096;
  static const uint32_t SEGMENT_HEADER_SIZE = sizeof(uint32_t) * 3 + sizeof(uint16_t) * 2;
  static const uint32_t SEGMENT_DATA_MAX_SIZE = SEGMENT_MAX_SIZE - SEGMENT_HEADER_SIZE;

  static const uint32_t NODE_HEADER_SIZE = sizeof(uint32_t) * 2 + sizeof(uint16_t) * 2;
  static const uint32_t NODE_DATA_MAX_SIZE = BLOCK_SIZE - NODE_HEADER_SIZE;

  static const uint32_t ENTRY_SIZE = sizeof(uint32_t) * 2;

public:
  TripleTable(const std::string& store_path, const std::string& table_name, BufferManager* buffer_manager);
  ~TripleTable();

  bool open();
  bool close();

  class Scanner {
  public:
    Scanner(TripleTable& table, Resource *res1, Resource *res2, Resource *res3);

    bool find();
    bool next();

  private:
    TripleTable& table;
    Resource *res1, *res2, *res3;

    bool load;
    uint32_t block_no;

    char segment_chunk[SEGMENT_MAX_SIZE];
    Segment* segment;
  };
  friend class Scanner;
  class Writer {
  public:
    Writer(TripleTable& table);
    ~Writer();

    bool put(const Resource& res1, const Resource& res2, const Resource& res3);
    bool flush();

  private:
    TripleTable& table;
  };
  friend class Writer;

  int count(uint32_t res_id);

private:
  // Segment file
  class HeapFile {
  public:
    HeapFile(const std::string& file_path, BufferManager* buffer_manager);
    ~HeapFile();

    bool open();
    bool close();

    BufferPage* appendNode();
    BufferPage* getNode(uint32_t block_no);
    void prefetchNode(uint32_t block_no);
    void updateNode(BufferPage* page, bool dirty, bool exclusive);

  private:
    #pragma pack(push, 1)
    struct Header {
      uint32_t version;
      uint32_t capacity;
      uint32_t block_size;
      uint32_t num_block;
      uint64_t file_size;
    };
    #pragma pack(pop)

    static const uint32_t HEADER_SIZE = sizeof(uint32_t) * 4 + sizeof(uint64_t);
    static const uint32_t FILE_GROWTH = 20;

    bool create(uint32_t capacity, uint32_t version);

    bool readHeader();
    bool writeHeader();

    std::string file_path;
    RandomRWFile file;
    Header* header;

    BufferManager* buffer_manager;
    Mutex mutex;
  };

  bool readHeader();
  bool writeHeader();

  bool readSegment(const rocksdb::Slice& key, std::string* segment);
  bool writeSegment(const rocksdb::Slice& key, const std::string& segment);
  
  bool writeData(uint32_t res1, const std::vector<uint32_t>& res2, const std::vector<uint32_t>& res3);
  bool writeData(uint32_t res1, uint32_t res2, uint32_t res3);
  bool flushData();

  std::string store_path;
  std::string table_name;
  rocksdb::DB *db;

  HeapFile data_file;
};


#endif
