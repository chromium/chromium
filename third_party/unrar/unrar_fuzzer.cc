// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/unrar/google/unrar_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_file_ = base::CreateAndOpenTemporaryFileInDir(temp_dir_.GetPath(),
                                                       &temp_file_path_);
  }

  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  base::File GetTempFile() const { return temp_file_.Duplicate(); }

  base::File InitRarFile() {
    return base::CreateAndOpenTemporaryFileInDir(temp_dir_.GetPath(),
                                                 &rar_file_path_);
  }

  void CleanUpRarFile() { base::DeleteFile(rar_file_path_); }

 private:
  base::ScopedTempDir temp_dir_;
  // RarReader uses a temporary file in the process of unpacking the RAR
  // archive. This is reused between runs.
  base::FilePath temp_file_path_;
  base::File temp_file_;
  // The file containing the fuzz input, to be parsed as a rar file. Unique per
  // run.
  base::FilePath rar_file_path_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // The common setup is static for efficiency.
  static Environment env;

  // Write the input data into a "rar" file.
  base::File rar_file = env.InitRarFile();
  rar_file.WriteAtCurrentPos(base::span<const uint8_t>(data, size));

  // Seek back to the beginning so `RarReader` reads from the start.
  rar_file.Seek(base::File::FROM_BEGIN, 0);

  // Fuzz the opening and parsing of the file.
  third_party_unrar::RarReader reader;
  if (!reader.Open(std::move(rar_file), env.GetTempFile())) {
    env.CleanUpRarFile();
    // Reject uninteresting inputs: do not add unopenable files to the corpus.
    return -1;
  }
  while (reader.ExtractNextEntry()) {
  }

  env.CleanUpRarFile();
  return 0;
}
