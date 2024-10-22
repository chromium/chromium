// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_fd_factory.h"

#include "base/files/file_util.h"

namespace wl {

TestFdFactory::TestFdFactory() {
  CHECK(temp_dir_.CreateUniqueTempDir());
}

TestFdFactory::~TestFdFactory() {
  CHECK(temp_dir_.Delete());
}

base::ScopedFD TestFdFactory::CreateFd() {
  base::FilePath dont_care;
  return base::CreateAndOpenFdForTemporaryFileInDir(temp_dir_.GetPath(),
                                                    &dont_care);
}

}  // namespace wl
