// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_FD_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_FD_FACTORY_H_

#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"

namespace wl {

class TestFdFactory {
 public:
  TestFdFactory();
  ~TestFdFactory();

  TestFdFactory(const TestFdFactory&) = delete;
  TestFdFactory& operator=(const TestFdFactory&) = delete;

  base::ScopedFD CreateFd();

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_FD_FACTORY_H_
