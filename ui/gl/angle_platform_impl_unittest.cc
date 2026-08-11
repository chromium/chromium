// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/angle_platform_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gl {
namespace {

TEST(ANGLEPlatformImplTest, PostWorkerTaskShutdownFallback) {
  // 1. Simulate PostTask failure.
  angle::SetPostTaskFailedForTesting(true);

  // 2. Call postWorkerTask directly.
  bool ran = false;
  angle::ANGLEPlatformImpl_postWorkerTask(
      nullptr,
      [](void* userData) {
        bool* ran_ptr = static_cast<bool*>(userData);
        *ran_ptr = true;
      },
      &ran);

  // 3. Verify that it ran synchronously.
  EXPECT_TRUE(ran);

  // Clean up.
  angle::SetPostTaskFailedForTesting(false);
}

}  // namespace
}  // namespace gl
