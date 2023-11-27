#include "wired_tiger.h"

WiredTigerConnection::WiredTigerConnection(const std::string& store_path) : store_path(store_path) {}

WiredTigerConnection::~WiredTigerConnection() {}

bool WiredTigerConnection::open() {
  if (wiredtiger_open(store_path.c_str(), NULL, "create,cache_size=500M,extensions=[/usr/local/lib/libwiredtiger_zlib.so]", &conn) != 0) {
    return false;
  }
  return true;
}

bool WiredTigerConnection::close() {
  conn->close(conn, NULL);
  return true;
}

WiredTigerTable::WiredTigerTable(const WiredTigerConnection& wt_conn, const std::string& table_name)
  : conn(wt_conn.conn), table_name(table_name) {}

WiredTigerTable::~WiredTigerTable() {}

bool WiredTigerTable::create(std::string key_format, std::string value_format) {
  conn->open_session(conn, NULL, NULL, &session);
  std::string uri = "table:" + table_name;
  std::string config = "block_compressor=zlib,checksum=off,key_format="+key_format+",value_format="+value_format;
  return session->create(session, uri.c_str(), config.c_str()) == 0;
}

bool WiredTigerTable::put(const std::string& key, const std::string& value) {
  return true;
}

bool WiredTigerTable::get(const std::string& key, std::string* value) {
  return true;
}

std::unique_ptr<WiredTigerTable::Iterator> WiredTigerTable::newIterator() {
  return std::unique_ptr<Iterator>(new Iterator(conn, table_name));
}

std::unique_ptr<WiredTigerTable::Writer> WiredTigerTable::newWriter() {
  return std::unique_ptr<Writer>(new Writer(conn, table_name));
}

std::unique_ptr<WiredTigerTable::BulkWriter> WiredTigerTable::newBulkWriter() {
  return std::unique_ptr<BulkWriter>(new BulkWriter(conn, table_name));
}

WiredTigerTable::Iterator::Iterator(WT_CONNECTION *conn, std::string table_name) {
  std::string uri = "table:" + table_name;
  conn->open_session(conn, NULL, NULL, &session);
  session->open_cursor(session, uri.c_str(), NULL, NULL, &this->cursor);
}

WiredTigerTable::Iterator::~Iterator() {
  cursor->close(cursor);
  session->close(session, NULL);
}

bool WiredTigerTable::Iterator::get(const std::string& key, std::string* value) {
  key_buf.data = key.c_str();
  key_buf.size = key.size();
  cursor->set_key(cursor, &key_buf);
  return cursor->search(cursor) == 0;
}

void WiredTigerTable::Iterator::seek(const std::string& key) {
  key_buf.data = key.c_str();
  key_buf.size = key.size();
  cursor->set_key(cursor, &key_buf);
  int exact;
  if(cursor->search_near(cursor, &exact) != 0) {
    is_valid = false;
    return;
  }
  if(exact < 0) {
    if(cursor->next(cursor) != 0) {
      is_valid = false;
      return;
    }
  }
  is_valid = true;
}

void WiredTigerTable::Iterator::lowerBound(const std::string& lower_bound) {
  key_buf.data = lower_bound.c_str();
  key_buf.size = lower_bound.size();
  cursor->set_key(cursor, &key_buf);
  cursor->bound(cursor, "bound=lower");
}

void WiredTigerTable::Iterator::upperBound(const std::string& upper_bound) {
  key_buf.data = upper_bound.c_str();
  key_buf.size = upper_bound.size();
  cursor->set_key(cursor, &key_buf);
  cursor->bound(cursor, "bound=upper");
}

void WiredTigerTable::Iterator::next() {
  if(cursor->next(cursor) != 0) {
    is_valid = false;
    return;
  }
  is_valid = true;
}
bool WiredTigerTable::Iterator::valid() {
  return is_valid;
}

std::string WiredTigerTable::Iterator::key() {
  cursor->get_key(cursor, &key_buf);
  return std::string(reinterpret_cast<const char*>(key_buf.data), key_buf.size);
}

std::string WiredTigerTable::Iterator::value() {
  cursor->get_value(cursor, &key_buf);
  return std::string(reinterpret_cast<const char*>(value_buf.data), value_buf.size);
}

WiredTigerTable::Writer::Writer(WT_CONNECTION *conn, std::string table_name) {
  std::string uri = "table:" + table_name;
  conn->open_session(conn, NULL, NULL, &session);
  session->open_cursor(session, uri.c_str(), NULL, NULL, &this->cursor);
}

WiredTigerTable::Writer::~Writer() {}

bool WiredTigerTable::Writer::append(const std::string& key, const std::string& value) {
  key_buf.data = key.c_str();
  key_buf.size = key.size();
  cursor->set_key(cursor, &key_buf);
  value_buf.data = value.c_str();
  value_buf.size = value.size();
  cursor->set_value(cursor, &value_buf);
  if(cursor->insert(cursor) != 0) {
    return false;
  }
  return true;
}

WiredTigerTable::BulkWriter::BulkWriter(WT_CONNECTION *conn, std::string table_name) {
  std::string uri = "table:" + table_name;
  conn->open_session(conn, NULL, NULL, &session);
  session->open_cursor(session, uri.c_str(), NULL, "bulk=true", &this->cursor);
}

WiredTigerTable::BulkWriter::~BulkWriter() {}

bool WiredTigerTable::BulkWriter::append(const std::string& key, const std::string& value) {
  key_buf.data = key.c_str();
  key_buf.size = key.size();
  cursor->set_key(cursor, &key_buf);
  value_buf.data = value.c_str();
  value_buf.size = value.size();
  cursor->set_value(cursor, &value_buf);
  if(cursor->insert(cursor) != 0) {
    return false;
  }
  return true;
}
