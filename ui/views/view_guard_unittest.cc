// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_guard.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "testing/gtest/include//gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace views {

using ViewGuardTest = ViewsTestBase;

TEST_F(ViewGuardTest, NoCrashIfGuardDeletedFirst) {
  View view;
  ViewGuard guard(view);
}

TEST_F(ViewGuardTest, CrashIfViewDeletedFirst) {
  std::unique_ptr<View> view = std::make_unique<View>();
  ViewGuard guard(*view);
  EXPECT_NOTREACHED_DEATH(view.reset());
}

}  // namespace views
