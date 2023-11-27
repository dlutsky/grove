#ifndef WIREDTIGER_H
#define WIREDTIGER_H

#include <string>
#include <memory>
#include <wiredtiger.h>

class WiredTigerTable;
class WiredTigerConnection {
public:
  WiredTigerConnection(const std::string& store_path);
  ~WiredTigerConnection();

  bool open();
  bool close();

private:
  friend class WiredTigerTable;

  WT_CONNECTION *conn;
  std::string store_path;
};

class WiredTigerTable {
public:
  WiredTigerTable(const WiredTigerConnection& wt_conn, const std::string& table_name);
  ~WiredTigerTable();

  bool create(std::string key_format, std::string value_format);

  class Iterator {
  public:
    ~Iterator();
    bool get(const std::string& key, std::string* value);
    void seek(const std::string& key);
    void lowerBound(const std::string& lower_bound);
    void upperBound(const std::string& upper_bound);
    void next();
    bool valid();
    std::string key();
    std::string value();

  private:
    Iterator(WT_CONNECTION *conn, std::string table_name);
    friend class WiredTigerTable;

    WT_SESSION *session;
    WT_CURSOR *cursor;
    WT_ITEM key_buf, value_buf;
    bool is_valid;
  };
  class Writer {
  public:
    Writer(WT_CONNECTION *conn, std::string table_name);
    ~Writer();
    bool append(const std::string& key, const std::string& value);

  private:
    friend class WiredTigerTable;

    WT_SESSION *session;
    WT_CURSOR *cursor;
    WT_ITEM key_buf, value_buf;
  };
  class BulkWriter {
  public:
    BulkWriter(WT_CONNECTION *conn, std::string table_name);
    ~BulkWriter();
    bool append(const std::string& key, const std::string& value);

  private:
    friend class WiredTigerTable;

    WT_SESSION *session;
    WT_CURSOR *cursor;
    WT_ITEM key_buf, value_buf;
  };

  bool put(const std::string& key, const std::string& value);
  bool get(const std::string& key, std::string* value);
  std::unique_ptr<Iterator> newIterator();
  std::unique_ptr<Writer> newWriter();
  std::unique_ptr<BulkWriter> newBulkWriter();

private:
  WT_CONNECTION *conn;
  WT_SESSION *session;
  std::string table_name;
};

#endif