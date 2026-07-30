// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "base/check.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_grammar_fuzzable.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"

using namespace sql_query_grammar;

DEFINE_PROTO_FUZZER(
    const fuzzable::sql_query_grammar::Printf& fuzzable_sql_printf) {
  std::string serialized;
  CHECK(fuzzable_sql_printf.SerializeToString(&serialized));
  sql_query_grammar::Printf sql_printf;
  CHECK(sql_printf.ParseFromString(serialized));
  std::string printf_str = sql_fuzzer::PrintfToString(sql_printf);
  // Convert printf command into runnable SQL query.
  printf_str = "SELECT " + printf_str + ";";

  if (::getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << "_________________________" << std::endl;
    std::cout << printf_str << std::endl;
    std::cout << "------------------------" << std::endl;
  }

  std::vector<std::string> queries;
  queries.push_back(printf_str);
  sql_fuzzer::RunSqlQueries(queries, ::getenv("LPM_SQLITE_TRACE"));
}
