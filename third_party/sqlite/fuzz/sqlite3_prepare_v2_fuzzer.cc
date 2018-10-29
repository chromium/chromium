// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

#include "third_party/sqlite/sqlite3.h"


static const std::array<uint8_t, 6> kBadKeyword{{'R', 'E', 'G', 'E', 'X', 'P'}};


bool checkForBadKeyword(const uint8_t* data, size_t size) {
  auto it = std::search(
      data, data + size, kBadKeyword.begin(), kBadKeyword.end(),
      [](char c1, char c2) { return std::toupper(c1) == std::toupper(c2); });

  if (it != data + size)
    return true;

  return false;
}


#if !defined(SQLITE_OMIT_PROGRESS_CALLBACK)

static int Progress(void *not_used_ptr) {
  return 1;
}

#endif  // !defined(SQLITE_OMIT_PROGRESS_CALLBACK)


// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2)
    return 0;

  if (checkForBadKeyword(data, size))
    return 0;

  sqlite3* db;
  int return_code = sqlite3_open_v2(
      "db.db",
      &db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, 0);

  if (SQLITE_OK != return_code)
    return 0;

  // Use first byte as random selector for other parameters.
  int selector = data[0];

#if !defined(SQLITE_OMIT_PROGRESS_CALLBACK)
  // To cover both cases when progress_handler is used and isn't used.
  if (selector & 1)
    sqlite3_progress_handler(db, 4, &Progress, NULL);
  else
    sqlite3_progress_handler(db, 0, NULL, NULL);

  // Remove least significant bit to make further usage of selector independent.
  selector >>= 1;
#endif  // !defined(SQLITE_OMIT_PROGRESS_CALLBACK)

  sqlite3_stmt* statement = NULL;
  int result = sqlite3_prepare_v2(db, reinterpret_cast<const char*>(data + 1),
                                  static_cast<int>(size - 1), &statement, NULL);
  if (result == SQLITE_OK) {
    // Use selector value to randomize number of iterations.
    for (int i = 0; i < selector; i++) {
      if (sqlite3_step(statement) != SQLITE_ROW)
        break;
    }

    sqlite3_finalize(statement);
  }

  sqlite3_close(db);
  return 0;
}
