// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/sqlite/fuzz/disabled_queries_parser.h"

namespace sql_fuzzer {

std::set<std::string> ParseDisabledQueries(std::string query_list) {
  // Trimming
  query_list.erase(query_list.find_last_not_of(" \t\n\r\f\v") + 1);
  query_list.erase(0, query_list.find_first_not_of(" \t\n\r\f\v"));
  std::set<std::string> ret;
  std::string curr_query;
  for (size_t i = 0; i < query_list.length(); i++) {
    if (query_list[i] == ',') {
      ret.insert(curr_query);
      curr_query.clear();
      continue;
    }
    curr_query += query_list[i];
  }
  if (curr_query.length() != 0) {
    // Add last query, which doesn't have a trailing comma
    ret.insert(curr_query);
  }
  return ret;
}

}  // namespace sql_fuzzer
