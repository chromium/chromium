// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/crash_id_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

class CrashIdHelperTest : public testing::Test {
 public:
  CrashIdHelperTest() = default;
  ~CrashIdHelperTest() override = default;

  std::string CurrentCrashId() {
    return CrashIdHelper::Get()->CurrentCrashId();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashIdHelperTest);
};

// This test verifies CurrentCrashId(). Ideally this would verify at
// crash_reporter::CrashKeyString, but that class isn't particularly test
// friendly (and the implementation varies depending upon compile time flags).
TEST_F(CrashIdHelperTest, Basic) {
  CrashIdHelper::RegisterMainThread(base::PlatformThread::CurrentId());

  const std::string id1 = "id";
  {
    auto scoper = CrashIdHelper::Get()->OnWillProcessMessages(id1);
    EXPECT_EQ(id1, CurrentCrashId());
  }

  // No assertions for empty as CurrentCrashId() DCHECKs there is at least
  // one id.

  const std::string id2 = "id2";
  {
    auto scoper = CrashIdHelper::Get()->OnWillProcessMessages(id2);
    EXPECT_EQ(id2, CurrentCrashId());

    {
      auto scoper2 = CrashIdHelper::Get()->OnWillProcessMessages(id1);
      EXPECT_EQ("id2>id", CurrentCrashId());
    }
    EXPECT_EQ("(N) id2", CurrentCrashId());
  }
  CrashIdHelper::RegisterMainThread(base::kInvalidThreadId);
}

}  // namespace gfx
