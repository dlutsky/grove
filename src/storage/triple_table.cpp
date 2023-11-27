#include <cstring>
#include "triple_table.h"

#include <iostream>

TripleTable::TripleTable(const std::string& store_path, const std::string& table_name, BufferManager* buffer_manager)
  : store_path(store_path), table_name(table_name), db(nullptr), data_file(store_path + "/" + table_name + "_data.graw", buffer_manager) {}

TripleTable::~TripleTable() {}

bool TripleTable::open() {
  if(db != nullptr) {
    delete db;
  }
  rocksdb::Options options;
  options.create_if_missing = true;
  rocksdb::Status status = rocksdb::DB::Open(options, store_path + "/" + table_name, &db);
  if(!status.ok()) {
    return false;
  }

  if(!this->data_file.open()) {
    return false;
  }

  return true;
}

bool TripleTable::close() {
  delete db;
  db = nullptr;

  if(!this->data_file.close()) {
    return false;
  }

  return true;
}

TripleTable::Scanner::Scanner(TripleTable& table, Resource *res1, Resource *res2, Resource *res3)
  : table(table), res1(res1), res2(res2), res3(res3), load(false), block_no(0) {
  segment = reinterpret_cast<Segment*>(segment_chunk);
}

bool TripleTable::Scanner::find() {
  Key key = { res1->id };
  std::string value;
  if(!this->table.readSegment(rocksdb::Slice(std::string(reinterpret_cast<char*>(&key), KEY_SIZE)), &value)){
    return false;
  }
  memcpy(segment_chunk, value.c_str(), value.size());
  return true;
}

bool TripleTable::Scanner::next() {
  if(block_no != 0) {
    BufferPage* page = this->table.data_file.getNode(block_no);
    char* block = page->getBlockData();
    Node* node = reinterpret_cast<Node*>(block);

    int i = 0;
    while(i < node->dsize) {
      const Entry *entry = reinterpret_cast<const Entry*>(node->data + i);
      res2->column.push_back(entry->x);
      res3->column.push_back(entry->y);
      i += ENTRY_SIZE;
    }

    block_no = node->next_block_no;
    this->table.data_file.updateNode(page, false, true);
    return true;
  }
  if(!load) {
    block_no = segment->first_block_no;
    int i = 0;
    while(i < segment->dsize) {
      const Entry *entry = reinterpret_cast<const Entry*>(segment->data + i);
      res2->column.push_back(entry->x);
      res3->column.push_back(entry->y);
      i += ENTRY_SIZE;
    }
    load = true;
    return true;
  }
  return false;
}


TripleTable::Writer::Writer(TripleTable& table)
  : table(table) {}

TripleTable::Writer::~Writer() {}

bool TripleTable::Writer::put(const Resource& res1, const Resource& res2, const Resource& res3) {
  if(!res2.column.empty()) {
    return this->table.writeData(res1.id, res2.column, res3.column);
  } else {
    return this->table.writeData(res1.id, res2.id, res3.id);
  }
}

bool TripleTable::Writer::flush() {
  return this->table.flushData();
}


int TripleTable::count(uint32_t res_id) {
  Key key = { res_id };
  std::string value;
  char segment_chunk[SEGMENT_MAX_SIZE];
  Segment* segment = reinterpret_cast<Segment*>(segment_chunk);
  if(!readSegment(rocksdb::Slice(std::string(reinterpret_cast<char*>(&key), KEY_SIZE)), &value)){
    return 0;
  }
  memcpy(segment_chunk, value.c_str(), value.size());
  return segment->count;
}

bool TripleTable::readSegment(const rocksdb::Slice& key, std::string* segment) {
  rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, segment);
  return status.ok();
}

bool TripleTable::writeSegment(const rocksdb::Slice& key, const std::string& segment) {
  rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, segment);
  return status.ok();
}

bool TripleTable::writeData(uint32_t res1, const std::vector<uint32_t>& res2, const std::vector<uint32_t>& res3) {
  Key key = { res1 };
  std::string value;
  char segment_chunk[SEGMENT_MAX_SIZE];
  Segment* segment = reinterpret_cast<Segment*>(segment_chunk);
  if(readSegment(rocksdb::Slice(std::string(reinterpret_cast<char*>(&key), KEY_SIZE)), &value)){
    memcpy(segment_chunk, value.c_str(), value.size());
  } else {
    segment->count = 0;
    segment->first_block_no = 0;
    segment->last_block_no = 0;
    segment->dtype = 0;
    segment->dsize = 0;
  }

  uint32_t last_block_no = segment->last_block_no;

  BufferPage* page;
  char* block;
  Node* node;
  if(last_block_no != 0) {
    page = this->data_file.getNode(last_block_no);
    block = page->getBlockData();
    node = reinterpret_cast<Node*>(block);
  } else {
    page = this->data_file.appendNode();
    segment->first_block_no = page->getBlockNo();
    block = page->getBlockData();
    node = reinterpret_cast<Node*>(block);
    node->block_no = page->getBlockNo();
    node->next_block_no = 0;
    node->dtype = 0;
    node->dsize = 0;
  }

  Entry entry;
  for(int i=0; i<res2.size(); i++) {
    entry.x = res2[i];
    entry.y = res3[i];

    if(node->dsize + ENTRY_SIZE >= NODE_DATA_MAX_SIZE) {
      BufferPage* new_page = this->data_file.appendNode();
      node->next_block_no = new_page->getBlockNo();
      this->data_file.updateNode(page, true, true);

      page = new_page;
      block = page->getBlockData();
      node = reinterpret_cast<Node*>(block);
      node->block_no = page->getBlockNo();
      node->next_block_no = 0;
      node->dtype = 0;
      node->dsize = 0;
    }

    memcpy(node->data + node->dsize, reinterpret_cast<char*>(&entry), ENTRY_SIZE);
    node->dsize += ENTRY_SIZE;
  }

  segment->count += res2.size();
  segment->last_block_no = page->getBlockNo();
  this->data_file.updateNode(page, true, true);

  writeSegment(rocksdb::Slice(std::string(reinterpret_cast<char*>(&key), KEY_SIZE)), std::string(segment_chunk, SEGMENT_HEADER_SIZE + segment->dsize));

  return true;
}

bool TripleTable::writeData(uint32_t res1, uint32_t res2, uint32_t res3) {
  Key key = { res1 };
  std::string value;
  char segment_chunk[SEGMENT_MAX_SIZE];
  Segment* segment = reinterpret_cast<Segment*>(segment_chunk);
  if(readSegment(rocksdb::Slice(std::string(reinterpret_cast<char*>(&key), KEY_SIZE)), &value)){
    memcpy(segment_chunk, value.c_str(), value.size());
  } else {
    segment->count = 0;
    segment->first_block_no = 0;
    segment->last_block_no = 0;
    segment->dtype = 0;
    segment->dsize = 0;
  }

  uint32_t last_block_no = segment->last_block_no;

  BufferPage* page;
  char* block;
  Node* node;
  if(last_block_no != 0) {
    page = this->data_file.getNode(last_block_no);
    block = page->getBlockData();
    node = reinterpret_cast<Node*>(block);
  } else {
    page = this->data_file.appendNode();
    segment->first_block_no = page->getBlockNo();
    block = page->getBlockData();
    node = reinterpret_cast<Node*>(block);
    node->block_no = page->getBlockNo();
    node->next_block_no = 0;
    node->dtype = 0;
    node->dsize = 0;
  }

  Entry entry;
  entry.x = res2;
  entry.y = res3;

  if(node->dsize + ENTRY_SIZE >= NODE_DATA_MAX_SIZE) {
    BufferPage* new_page = this->data_file.appendNode();
    node->next_block_no = new_page->getBlockNo();
    this->data_file.updateNode(page, true, true);

    page = new_page;
    block = page->getBlockData();
    node = reinterpret_cast<Node*>(block);
    node->block_no = page->getBlockNo();
    node->next_block_no = 0;
    node->dtype = 0;
    node->dsize = 0;
  }

  memcpy(node->data + node->dsize, reinterpret_cast<char*>(&entry), ENTRY_SIZE);
  node->dsize += ENTRY_SIZE;

  segment->count += 1;
  segment->last_block_no = page->getBlockNo();
  this->data_file.updateNode(page, true, true);

  writeSegment(rocksdb::Slice(std::string(reinterpret_cast<char*>(&key), KEY_SIZE)), std::string(segment_chunk, SEGMENT_HEADER_SIZE + segment->dsize));

  return true;
}

bool TripleTable::flushData() {
  return true;
}




// Heap File
TripleTable::HeapFile::HeapFile(const std::string& file_path, BufferManager* buffer_manager)
  : file_path(file_path), file(file_path), buffer_manager(buffer_manager) {}

TripleTable::HeapFile::~HeapFile() {}

bool TripleTable::HeapFile::open() {
  if(!File::exist(file_path)) {
    if(!create(FILE_GROWTH, 0)) {
      return false;
    }
  }
  // file.open();
  if(!file.open(O_RDWR)) {
    return false;
  }

  header = reinterpret_cast<Header*>(new char[HEADER_SIZE]);
  if(!readHeader()) {
    return false;
  }
  return true;
}

bool TripleTable::HeapFile::close() {
  writeHeader();
  delete[] reinterpret_cast<char*>(header);
  this->buffer_manager->flushFile(&file, true);
  file.close();
  return true;
}

BufferPage* TripleTable::HeapFile::appendNode() {
  BufferPage* page = nullptr;
  mutex.lock();
  uint32_t block_no = header->num_block;
  header->num_block++;
  uint64_t fsize = header->num_block * BLOCK_SIZE;
  if(fsize > header->file_size) {
    fsize = (header->num_block + FILE_GROWTH) * BLOCK_SIZE;
    file.truncate(fsize);
    header->file_size = fsize;
  }
  page = buffer_manager->allocBufferPage(&this->file, block_no, false);
  mutex.unlock();
  return page;
}

BufferPage* TripleTable::HeapFile::getNode(uint32_t block_no) {
  return this->buffer_manager->getBufferPage(&this->file, block_no, true);
}

void TripleTable::HeapFile::prefetchNode(uint32_t block_no) {
  this->file.prefetch(block_no * BLOCK_SIZE, BLOCK_SIZE);
}

void TripleTable::HeapFile::updateNode(BufferPage* page, bool dirty, bool exclusive) {
  this->buffer_manager->unfixBufferPage(page, dirty, exclusive);
}

bool TripleTable::HeapFile::create(uint32_t capacity, uint32_t version) {
  if(capacity < 1) {
    capacity = FILE_GROWTH;
  }
  file.open(O_WRONLY|O_CREAT);
  header = reinterpret_cast<Header*>(new char[HEADER_SIZE]);
  header->version = version;
  header->capacity = capacity;
  header->block_size = BLOCK_SIZE;
  header->num_block = 1;
  header->file_size = (header->capacity) * BLOCK_SIZE;
  file.truncate(header->file_size);
  writeHeader();
  delete[] reinterpret_cast<char*>(header);
  file.close();
  return true;
}

bool TripleTable::HeapFile::readHeader() {
  this->file.read(reinterpret_cast<char*>(header), HEADER_SIZE, 0);
  return true;
}

bool TripleTable::HeapFile::writeHeader() {
  this->file.write(reinterpret_cast<const char*>(header), HEADER_SIZE, 0);
  return true;
}
