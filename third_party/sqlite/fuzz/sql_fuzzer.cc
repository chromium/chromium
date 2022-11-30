// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/disabled_queries_parser.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"

using namespace sql_query_grammar;

// Environment variable LPM_DUMP_NATIVE_INPUT can be used to print the
// SQL queries used in the Clusterfuzz test case.

// TODO(mpdenton): Fuzzing tasks
// 1. Definitely fix a lot of the syntax errors that SQlite spits out
// 2. CORPUS Indexes on expressions (https://www.sqlite.org/expridx.html) and
// other places using functions on columns???
// 3. Generate a nice big random, well-formed corpus.
// 4. Possibly very difficult for fuzzer to find certain areas of code, because
// some protobufs need to be mutated together. For example, an index on an
// expression is useless to change, if you don't change the SELECTs that use
// that expression. May need to create a mechanism for the protobufs to
// "register" (in the c++ fuzzer) expressions being used for certain purposes,
// and then protobufs can simple reference those expressions later (similarly to
// columns or tables, with just an index). This should be added if coverage
// shows it is the case.
// 5. Add coverage for the rest of the pragmas
// 6. Make sure defensive config is off
// 7. Fuzz the recover extension from the third patch
// 8. Temp-file database, for better fuzzing of VACUUM and journalling.

DEFINE_BINARY_PROTO_FUZZER(const SQLQueries& sql_queries) {
  char* skip_queries = ::getenv("SQL_SKIP_QUERIES");
  if (skip_queries) {
    sql_fuzzer::SetDisabledQueries(
        sql_fuzzer::ParseDisabledQueries(skip_queries));
  }

  std::vector<std::string> queries = sql_fuzzer::SQLQueriesToVec(sql_queries);

  if (::getenv("LPM_DUMP_NATIVE_INPUT") && queries.size() != 0) {
    std::cout << "_________________________" << std::endl;
    for (std::string query : queries) {
      if (query == ";")
        continue;
      std::cout << query << std::endl;
    }
    std::cout << "------------------------" << std::endl;
  }

  sql_fuzzer::RunSqlQueries(queries, ::getenv("LPM_SQLITE_TRACE"));
}
