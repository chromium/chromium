/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <cstdint>
#include <vector>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/status/statusor.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "leveldb/env.h"  // from @com_google_leveldb
#include "leveldb/options.h"  // from @com_google_leveldb
#include "leveldb/table.h"  // from @com_google_leveldb
#include "pybind11/cast.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11_abseil/absl_casters.h"  // from @pybind11_abseil
#include "pybind11_abseil/status_casters.h"  // from @pybind11_abseil

namespace pybind11 {

PYBIND11_MODULE(leveldb_testing_utils, m) {
  google::ImportStatusModule();

  m.def(
      "leveldb_table_to_pair_list",
      [](const std::string fname, bool compressed)
          -> absl::StatusOr<std::vector<std::pair<bytes, bytes>>> {
        auto* env = leveldb::Env::Default();
        leveldb::RandomAccessFile* file;
        if (!env->NewRandomAccessFile(fname, &file).ok()) {
          return absl::InternalError(absl::StrFormat(
              "Failed to create RandomAccessFile at %s", fname));
        }
        auto unique_file = absl::WrapUnique(file);
        uint64_t file_size;
        if (!env->GetFileSize(fname, &file_size).ok()) {
          return absl::InternalError(
              absl::StrFormat("Failed to get file size at %s", fname));
        }
        leveldb::Options options;
        options.compression =
            compressed ? leveldb::kSnappyCompression : leveldb::kNoCompression;

        leveldb::Table* table;
        if (!leveldb::Table::Open(options, file, file_size, &table).ok()) {
          return absl::InternalError("Failed to open table");
        }
        auto unique_table = absl::WrapUnique(table);
        auto table_iterator =
            absl::WrapUnique(table->NewIterator(leveldb::ReadOptions()));
        table_iterator->SeekToFirst();

        std::vector<std::pair<bytes, bytes>> result;
        for (; table_iterator->Valid(); table_iterator->Next()) {
          result.push_back(
              std::make_pair(bytes(table_iterator->key().ToString()),
                             bytes(table_iterator->value().ToString())));
        }
        return result;
      },
      arg("buffer"), arg("compressed"));
}

}  // namespace pybind11
