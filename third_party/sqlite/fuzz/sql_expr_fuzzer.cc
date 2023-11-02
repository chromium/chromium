// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"
#include "third_party/sqlite/fuzz/sql_run_queries.h"

using namespace sql_query_grammar;

DEFINE_BINARY_PROTO_FUZZER(const Expr& expr) {
  std::string expr_str = sql_fuzzer::ExprToString(expr);
  // Convert printf command into runnable SQL query.
  expr_str = "SELECT " + expr_str + ";";

  if (::getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << "_________________________" << std::endl;
    std::cout << expr_str << std::endl;
    std::cout << "------------------------" << std::endl;
  }

  std::vector<std::string> queries;
  queries.push_back(expr_str);
  sql_fuzzer::RunSqlQueries(queries, ::getenv("LPM_SQLITE_TRACE"));
}
