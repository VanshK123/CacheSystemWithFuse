#include "metadata_store.h"
#include <sqlite3.h>
#include <iostream>

MetadataStore::MetadataStore(const std::string& db_path)
    : db_path_(db_path), db_handle_(nullptr) {}

MetadataStore::~MetadataStore() {
    if (db_handle_) {
        sqlite3_close(static_cast<sqlite3*>(db_handle_));
    }
}

bool MetadataStore::init() {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << '\n';
        return false;
    }
    db_handle_ = db;
    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS metadata ("
        "path TEXT PRIMARY KEY,"
        "local_path TEXT,"
        "size INTEGER,"
        "timestamp INTEGER,"
        "last_accessed INTEGER,"
        "dirty INTEGER"
        ");";
    char* errmsg = nullptr;
    if (sqlite3_exec(db, create_sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::cerr << "Failed to create table: " << errmsg << '\n';
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

std::optional<CacheMetadata> MetadataStore::get(const std::string& path) {
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    const char* sql = "SELECT local_path, size, timestamp, last_accessed, dirty FROM metadata WHERE path=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    CacheMetadata meta;
    meta.path = path;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        meta.local_path     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.size           = sqlite3_column_int64(stmt, 1);
        meta.timestamp      = static_cast<std::time_t>(sqlite3_column_int64(stmt, 2));
        meta.last_accessed  = static_cast<std::time_t>(sqlite3_column_int64(stmt, 3));
        meta.dirty          = sqlite3_column_int(stmt, 4) != 0;
        sqlite3_finalize(stmt);
        return meta;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool MetadataStore::put(const CacheMetadata& meta) {
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    const char* sql =
        "INSERT INTO metadata (path, local_path, size, timestamp, last_accessed, dirty) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(path) DO UPDATE SET "
        "local_path=excluded.local_path, size=excluded.size, "
        "timestamp=excluded.timestamp, last_accessed=excluded.last_accessed, dirty=excluded.dirty;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, meta.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, meta.local_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(meta.size));
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(meta.timestamp));
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(meta.last_accessed));
    sqlite3_bind_int(stmt, 6, meta.dirty ? 1 : 0);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::updateAccessTime(const std::string& path, std::time_t last_accessed) {
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    const char* sql = "UPDATE metadata SET last_accessed=? WHERE path=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(last_accessed));
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::markDirty(const std::string& path, bool dirty) {
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    const char* sql = "UPDATE metadata SET dirty=? WHERE path=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, dirty ? 1 : 0);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool MetadataStore::remove(const std::string& path) {
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    const char* sql = "DELETE FROM metadata WHERE path=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<CacheMetadata> MetadataStore::allEntries() {
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    const char* sql = "SELECT path, local_path, size, timestamp, last_accessed, dirty FROM metadata;";
    sqlite3_stmt* stmt;
    std::vector<CacheMetadata> entries;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CacheMetadata meta;
        meta.path          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.local_path    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        meta.size          = sqlite3_column_int64(stmt, 2);
        meta.timestamp     = static_cast<std::time_t>(sqlite3_column_int64(stmt, 3));
        meta.last_accessed = static_cast<std::time_t>(sqlite3_column_int64(stmt, 4));
        meta.dirty         = sqlite3_column_int(stmt, 5) != 0;
        entries.push_back(meta);
    }
    sqlite3_finalize(stmt);
    return entries;
}

void MetadataStore::cleanup() {
    if (!db_handle_) return;
    const char* sql = "DROP TABLE IF EXISTS metadata;";
    char* errmsg = nullptr;
    sqlite3_exec(static_cast<sqlite3*>(db_handle_), sql, nullptr, nullptr, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(static_cast<sqlite3*>(db_handle_));
    db_handle_ = nullptr;
}
