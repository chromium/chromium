// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unused because SQLite is so serialized and concurrency-unfriendly that this
// really wouldn't test anything.

#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/disabled_queries_parser.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"
#include "third_party/sqlite/sqlite3.h"

using namespace sql_query_grammar;

namespace {
constexpr int kNumThreads = 4;  // Must change with MultipleSQLQueries protobuf.
}

DEFINE_BINARY_PROTO_FUZZER(const MultipleSQLQueries& multiple_sql_queries) {
  char* skip_queries = ::getenv("SQL_SKIP_QUERIES");
  if (skip_queries) {
    sql_fuzzer::SetDisabledQueries(
        sql_fuzzer::ParseDisabledQueries(skip_queries));
  }

  assert(multiple_sql_queries.GetDescriptor()->field_count() == kNumThreads);

  sqlite3* db = sql_fuzzer::InitConnectionForFuzzing();
  if (!db)
    return;

  if (::getenv("LPM_SQLITE_TRACE")) {
    sql_fuzzer::EnableSqliteTracing(db);
  }

  std::vector<std::string> query_strs[kNumThreads];
  query_strs[0] = sql_fuzzer::SQLQueriesToVec(multiple_sql_queries.queries1());
  query_strs[1] = sql_fuzzer::SQLQueriesToVec(multiple_sql_queries.queries2());
  query_strs[2] = sql_fuzzer::SQLQueriesToVec(multiple_sql_queries.queries3());
  query_strs[3] = sql_fuzzer::SQLQueriesToVec(multiple_sql_queries.queries4());

  if (::getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << "_________________________" << std::endl;
    for (int i = 0; i < kNumThreads; i++) {
      std::cout << "Thread " << i << ":" << std::endl;
      for (std::string query : query_strs[i]) {
        if (query == ";")
          continue;
        std::cout << query << std::endl;
      }
    }
    std::cout << "------------------------" << std::endl;
  }

  int num_threads_started = 0;
  std::mutex m;
  std::condition_variable cv;

  std::vector<std::thread> threads;

  auto to_run = [&](std::vector<std::string> queries) {
    // Wait for all the threads to start.
    std::unique_lock<std::mutex> lk(m);
    num_threads_started++;
    cv.notify_all();
    cv.wait(lk, [&] { return num_threads_started == kNumThreads; });
    m.unlock();

    sql_fuzzer::RunSqlQueriesOnConnection(db, queries);
  };

  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back(to_run, query_strs[i]);
  }

  for (int i = 0; i < kNumThreads; i++) {
    threads[i].join();
  }

  sql_fuzzer::CloseConnection(db);
}
