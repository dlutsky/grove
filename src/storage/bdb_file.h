#ifndef BDB_FILE_H
#define BDB_FILE_H

#include <string>
#include <memory>
#include <db_cxx.h>


class BDBFile {
public:
  BDBFile(const std::string& file_path);
  ~BDBFile();

  bool open();
  bool close();

  class Iterator {
  public:
    ~Iterator();
    bool get(const std::string& key, std::string* value);
    void seek(const std::string& key);
    void next();
    bool valid();
    std::string key();
    std::string value();

  private:
    Iterator(Db* file);
    friend class BDBFile;

    static const uint32_t BUFFER_SIZE = 4096;

    Dbc* cursor;
    char buf[BUFFER_SIZE];
    Dbt key_buf, data_buf;
    Dbt key_entry, data_entry;
    DbMultipleKeyDataIterator* ptrkd;
    bool is_valid;
  };
  class WriteBatch {
  public:
    WriteBatch();
    ~WriteBatch();
    bool append(const std::string& key, const std::string& value);
    int count();

  private:
    friend class BDBFile;

    static const uint32_t UPDATES_PER_BULK_PUT = 100;
    static const uint32_t MAX_DATA_SIZE = 50;
    static const uint32_t BUFFER_SIZE = UPDATES_PER_BULK_PUT * MAX_DATA_SIZE * 1024;

    char buf[BUFFER_SIZE];
    Dbt key_entry, data_entry;
    DbMultipleKeyDataBuilder* ptrkd;
    int cnt;
  };

  bool put(const std::string& key, const std::string& value);
  bool put(WriteBatch& batch);
  bool flush();

  bool get(const std::string& key, std::string* value);
  std::unique_ptr<Iterator> newIterator();

private:
  Db* file;
  std::string file_path;
};




#endif
