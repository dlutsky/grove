#include <cstring>
#include "bdb_file.h"


BDBFile::BDBFile(const std::string& file_path) : file_path(file_path) {
  file = new Db(NULL, 0);
}

BDBFile::~BDBFile() {
  delete file;
}

bool BDBFile::open() {
  try {
    file->open(
        NULL,
        file_path.c_str(),
        NULL,
        DB_BTREE,
        DB_CREATE,
        0664
    );
  } catch (DbException &e) {
    return false;
  }
  return true;
}

bool BDBFile::close() {
  return file->close(0) == 0;
}

bool BDBFile::put(const std::string& key, const std::string& value) {
  Dbt key_entry((void*)const_cast<char*>(key.data()), key.size());
  Dbt data_entry((void*)const_cast<char*>(value.data()), value.size());
  if(file->put(NULL, &key_entry, &data_entry, 0) != 0) {
    return false;
  }
  return true;
}

bool BDBFile::put(WriteBatch& batch) {
  if(file->put(NULL, &batch.key_entry, &batch.data_entry, DB_MULTIPLE_KEY) != 0) {
    return false;
  }
  batch.cnt = 0;
  return true;
}

bool BDBFile::flush() {
  return file->sync(0) == 0;
}

bool BDBFile::get(const std::string& key, std::string* value) {
  Dbt key_entry((void*)const_cast<char*>(key.data()), key.size());
  Dbt data_entry;
  if(file->get(NULL, &key_entry, &data_entry, 0) != 0) {
    return false;
  }
  value->assign((char*)data_entry.get_data(), data_entry.get_size());
  return true;
}

std::unique_ptr<BDBFile::Iterator> BDBFile::newIterator() {
  return std::unique_ptr<Iterator>(new Iterator(file));
}

BDBFile::Iterator::Iterator(Db* file)
  : is_valid(false) {
  file->cursor(NULL, &cursor, 0);
  /*
  memset(buf, 0, BUFFER_SIZE);
	data_buf.set_flags(DB_DBT_USERMEM);
	data_buf.set_data(buf);
	data_buf.set_ulen(BUFFER_SIZE);
	data_buf.set_size(BUFFER_SIZE);
  ptrkd = new DbMultipleKeyDataIterator(data_buf);
  */
}

BDBFile::Iterator::~Iterator() {
  cursor->close();
  // delete ptrkd;
}

bool BDBFile::Iterator::get(const std::string& key, std::string* value) {
  key_entry = Dbt((void*)const_cast<char*>(key.data()), key.size());
  Dbt value_entry;
  if(cursor->get(&key_entry, &value_entry, DB_SET) != 0) {
    return false;
  }
  value->assign((char*)value_entry.get_data(), value_entry.get_size());
  return true;
}

void BDBFile::Iterator::seek(const std::string& key) {
  key_entry = Dbt((void*)const_cast<char*>(key.data()), key.size());
  if(cursor->get(&key_entry, &data_entry, DB_SET_RANGE) != 0) {
    is_valid = false;
    return;
  }
  is_valid = true;
  /*
  key_buf = Dbt((void*)const_cast<char*>(key.data()), key.size());
  if(cursor->get(&key_buf, &data_buf, DB_SET_RANGE|DB_MULTIPLE_KEY) != 0) {
    is_valid = false;
    return;
  }
  if(!ptrkd->next(key_entry, data_entry)){
    is_valid = false;
  }
  is_valid = true;
  */
}

void BDBFile::Iterator::next() {
  if(cursor->get(&key_entry, &data_entry, DB_NEXT) != 0) {
    is_valid = false;
    return;
  }
  is_valid = true;
  /*
  if(!ptrkd->next(key_entry, data_entry)){
    if(cursor->get(&key_buf, &data_buf, DB_NEXT) != 0) {
      is_valid = false;
      return;
    }
    delete ptrkd;
    ptrkd = new DbMultipleKeyDataIterator(data_buf);
    if(!ptrkd->next(key_entry, data_entry)){
      is_valid = false;
    }
    is_valid = true;
  } else {
    is_valid = true;
  }
  */
}
bool BDBFile::Iterator::valid() {
  return is_valid;
}

std::string BDBFile::Iterator::key() {
  return std::string((char*)key_entry.get_data(), key_entry.get_size());
}

std::string BDBFile::Iterator::value() {
  return std::string((char*)data_entry.get_data(), data_entry.get_size());
}

BDBFile::WriteBatch::WriteBatch() {
  memset(buf, 0, BUFFER_SIZE);
  key_entry.set_ulen(BUFFER_SIZE);
  key_entry.set_flags(DB_DBT_USERMEM | DB_DBT_BULK);
  key_entry.set_data((void*)buf);

  ptrkd = nullptr;
  cnt = 0;
}

BDBFile::WriteBatch::~WriteBatch() {
  delete ptrkd;
}

bool BDBFile::WriteBatch::append(const std::string& key, const std::string& value) {
  if(cnt == UPDATES_PER_BULK_PUT) {
    return false;
  }
  if(cnt == 0) {
    if(ptrkd != nullptr) {
      delete ptrkd;
    }
    ptrkd = new DbMultipleKeyDataBuilder(key_entry);
  }
  ptrkd->append((void*)const_cast<char*>(key.data()), key.size(), (void*)const_cast<char*>(value.data()), value.size());
  ++cnt;
  return true;
}

int BDBFile::WriteBatch::count() {
  return cnt;
}