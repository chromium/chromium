// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/angle_platform_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gl {
namespace {

TEST(ANGLEPlatformImplTest, PostWorkerTask) {
  base::WaitableEvent event;
  bool ran = false;
  struct Context {
    raw_ptr<base::WaitableEvent> event;
    raw_ptr<bool> ran;
  } context{&event, &ran};

  angle::ANGLEPlatformImpl_postWorkerTask(
      nullptr,
      [](void* user_data) {
        auto* ctx = static_cast<Context*>(user_data);
        *ctx->ran = true;
        ctx->event->Signal();
      },
      &context);
  event.Wait();
  EXPECT_TRUE(ran);
}

}  // namespace
}  // namespace gl
