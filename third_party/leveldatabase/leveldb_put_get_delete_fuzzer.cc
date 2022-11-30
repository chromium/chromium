// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

#define FUZZING_ASSERT(condition)                                      \
  if (!(condition)) {                                                  \
    fprintf(stderr, "%s\n", "Fuzzing Assertion Failure: " #condition); \
    abort();                                                           \
  }

using leveldb::DB;
using leveldb::Env;
using leveldb::Options;
using leveldb::ReadOptions;
using leveldb::Slice;
using leveldb::Status;
using leveldb::WriteOptions;

// We need to use keys and values both shorter and longer than 128 bytes in
// order to cover both fast and slow paths in DecodeEntry.
static constexpr size_t kMaxKeyLen = 256;
static constexpr size_t kMaxValueLen = 256;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Reject too long inputs as they may cause non actionable timeouts issues.
  if (size > 128 * 1024)
    return 0;

  Env* mem_env = NewMemEnv(Env::Default());
  FuzzedDataProvider data_provider(data, size);

  Options open_opts;
  open_opts.create_if_missing = true;
  open_opts.paranoid_checks = data_provider.ConsumeBool();
  open_opts.reuse_logs = false;
  open_opts.env = mem_env;

  ReadOptions read_opts;
  read_opts.verify_checksums = data_provider.ConsumeBool();
  read_opts.fill_cache = data_provider.ConsumeBool();

  WriteOptions write_opts;
  write_opts.sync = data_provider.ConsumeBool();

  DB* db = nullptr;
  Status open_status = DB::Open(open_opts, "leveldbfuzztest", &db);
  FUZZING_ASSERT(open_status.ok());

  // Put a couple constant values which must be successfully written.
  FUZZING_ASSERT(db->Put(write_opts, "key1", "val1").ok());
  FUZZING_ASSERT(db->Put(write_opts, "key2", "val2").ok());

  // Split the data into a sequence of (key, value) strings and put those in.
  // Also collect both keys and values to be used as keys for retrieval below.
  std::vector<std::string> strings_used;
  while (data_provider.remaining_bytes()) {
    std::string key = data_provider.ConsumeRandomLengthString(kMaxKeyLen);
    std::string value = data_provider.ConsumeRandomLengthString(kMaxValueLen);
    db->Put(write_opts, key, value);
    strings_used.push_back(key);
    strings_used.push_back(value);
  }

  // Use all the strings we have extracted from the data previously as the keys.
  for (const auto& key : strings_used) {
    std::string db_value;
    db->Get(read_opts, Slice(key.data(), key.size()), &db_value);
  }

  // Delete all keys previously written to the database.
  for (const auto& key : strings_used) {
    db->Delete(write_opts, Slice(key.data(), key.size()));
  }

  delete db;
  db = nullptr;
  delete mem_env;
  mem_env = nullptr;
  return 0;
}
