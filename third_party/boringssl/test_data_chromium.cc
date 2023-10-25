// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

// BoringSSL requires a GetTestData function to pick up test data files. By
// default, BoringSSL generates a source file with the data embedded, but this
// exceeds the limit for the _CheckForTooLargeFiles presubmit check.
std::string GetTestData(const char *path) {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.AppendASCII("third_party/boringssl/src");
  file_path = file_path.AppendASCII(path);

  std::string result;
  CHECK(base::ReadFileToString(file_path, &result));
  return result;
}
