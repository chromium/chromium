// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_IDLE_TEST_UTILS_H_
#define UI_BASE_TEST_IDLE_TEST_UTILS_H_

#include <memory>

namespace ui {

class IdleTimeProvider;

namespace test {

class ScopedIdleProviderForTest {
 public:
  explicit ScopedIdleProviderForTest(
      std::unique_ptr<IdleTimeProvider> idle_time_provider);
  ~ScopedIdleProviderForTest();
};

}  // namespace test

}  // namespace ui

#endif  // UI_BASE_TEST_IDLE_TEST_UTILS_H_
