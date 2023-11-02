// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from sqlite's ossfuzz.c

#include <cstdlib>
#include <iostream>  // TODO(mpdenton) remove
#include <string>
#include <vector>

#include "third_party/sqlite/sqlite3.h"

namespace sql_fuzzer {

namespace {
constexpr int kMaxNumRows = 10;
constexpr int kMaxNumColumns = 10;

sqlite3_int64 killTime;

/* Return the current real-world time in milliseconds since the
** Julian epoch (-4714-11-24).
*/
static sqlite3_int64 timeOfDay(void) {
  static sqlite3_vfs* clockVfs = 0;
  sqlite3_int64 t;
  if (clockVfs == 0) {
    clockVfs = sqlite3_vfs_find(0);
    if (clockVfs == 0)
      return 0;
  }
  if (clockVfs->iVersion >= 2 && clockVfs->xCurrentTimeInt64 != 0) {
    clockVfs->xCurrentTimeInt64(clockVfs, &t);
  } else {
    double r;
    clockVfs->xCurrentTime(clockVfs, &r);
    t = (sqlite3_int64)(r * 86400000.0);
  }
  return t;
}

int progress_handler(void*) {
  sqlite3_int64 iNow = timeOfDay();
  int rc = iNow >= killTime;
  return rc;
}
}  // namespace

void RunSqlQueriesOnSameDB() {
  // TODO(mpdenton) unimplemented
}

sqlite3* InitConnectionForFuzzing() {
  int rc;       // Return code from various interfaces.
  sqlite3* db;  // Sqlite db.

  rc = sqlite3_initialize();
  if (rc) {
    std::cerr << "Failed initialization. " << std::endl;
    return nullptr;
  }

  // Open the database connection.  Only use an in-memory database.
  rc = sqlite3_open_v2(
      "fuzz.db", &db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, 0);
  if (rc) {
    std::cerr << "Failed to open DB. " << std::endl;
    return nullptr;
  }

  // Enables foreign key constraints
  sqlite3_db_config(db, SQLITE_DBCONFIG_ENABLE_FKEY, 1, &rc);

  // sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 0, &rc); // TODO(pwnall)

  return db;
}

void EnableSqliteTracing(sqlite3* db) {
  sqlite3_exec(db, "PRAGMA vdbe_debug=ON", 0, 0, 0);
}

void CloseConnection(sqlite3* db) {
  // Cleanup and return.
  sqlite3_exec(db, "PRAGMA temp_store_directory=''", 0, 0, 0);
  sqlite3_close(db);
}

void RunSqlQueriesOnConnection(sqlite3* db, std::vector<std::string> queries) {
  int rc;
  for (size_t i = 0; i < queries.size(); i++) {
    // Run each query one by one.
    // First, compile the query.
    sqlite3_stmt* stmt;
    const char* pzTail;
    rc = sqlite3_prepare_v2(db, queries[i].c_str(), -1, &stmt, &pzTail);
    if (rc != SQLITE_OK) {
      if (::getenv("PRINT_SQLITE_ERRORS")) {
        std::cerr << "Could not compile: " << queries[i] << std::endl;
        std::cerr << "Error message from db: " << sqlite3_errmsg(db)
                  << std::endl;
        std::cerr << "-----------------------------" << std::endl;
      }
      continue;
    }

    // No sqlite3_bind.

    // Reset progress callback for every query. Timeout after 1 second.
    // ClusterFuzz timeouts are not useful, so we try to avoid them.
    // This will hopefully make Clusterfuzz find better, smaller SELECT
    // statements.
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
    killTime = timeOfDay() + 1000;
    sqlite3_progress_handler(db, 100, progress_handler, nullptr);
#endif

    // Now run the compiled query.
    int col_cnt = sqlite3_column_count(stmt);
    int count = 0;
    rc = SQLITE_ROW;
    while (rc == SQLITE_ROW && count++ <= kMaxNumRows) {
      rc = sqlite3_step(stmt);
      if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        if (::getenv("PRINT_SQLITE_ERRORS")) {
          std::cerr << "Step problem: " << queries[i] << std::endl;
          std::cerr << "Error message from db: " << sqlite3_errmsg(db)
                    << std::endl;
          std::cerr << "-----------------------------" << std::endl;
        }
        goto free_stmt;
      }
      // Loop through the columns to catch a little bit more coverage.
      for (int i = 0; i < col_cnt && i < kMaxNumColumns; i++) {
        switch (sqlite3_column_type(stmt, i)) {
          case SQLITE_INTEGER:
            sqlite3_column_int(stmt, i);
            break;
          case SQLITE_FLOAT:
            sqlite3_column_double(stmt, i);
            break;
          case SQLITE_TEXT:
            sqlite3_column_text(stmt, i);
            break;
          case SQLITE_BLOB:
            sqlite3_column_blob(stmt, i);
            break;
          default:
            break;
        }
      }
    }

    // Finalize the query
  free_stmt:
    sqlite3_finalize(stmt);
  }
}

void RunSqlQueries(std::vector<std::string> queries, bool enable_tracing) {
  sqlite3* db = InitConnectionForFuzzing();
  if (enable_tracing)
    EnableSqliteTracing(db);

  RunSqlQueriesOnConnection(db, queries);

  CloseConnection(db);
}

}  // namespace sql_fuzzer
