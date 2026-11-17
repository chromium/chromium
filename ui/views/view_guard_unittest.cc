// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_guard.h"

#include <memory>

#include "base/not_fatal_until.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_not_fatal_until_severity_override.h"
#include "testing/gtest/include/gtest/gtest.h"
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

TEST_F(ViewGuardTest, NotFatalUntilDoesNotDangleWhenNonFatal) {
  // TODO(crbug.com/578997426): Update milestone when M165 is reached.
  if (base::internal::ScopedNotFatalUntilSeverityOverride::IsMilestoneReached(
          base::NotFatalUntil::M165)) {
    GTEST_SKIP() << "M165 reached.";
  }
  base::internal::ScopedNotFatalUntilSeverityOverride non_fatal_override(
      base::NotFatalUntil::M165, /*is_fatal=*/false);
  std::unique_ptr<View> view = std::make_unique<View>();
  ViewGuard guard(*view, base::NotFatalUntil::M165);
  view.reset();
}

}  // namespace views
