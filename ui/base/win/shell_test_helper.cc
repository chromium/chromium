// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

int main(int argc, char* argv[]) {
  // Write a sentinel file to the current directory to indicate success.
  if (!base::WriteFile(base::FilePath(L"executed.txt"),
                       base::as_byte_span("EXECUTED"))) {
    return 1;
  }

  return 0;
}
