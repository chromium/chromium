// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_wm_move_loop.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

bool IsRawButtonReleaseSelected(x11::Connection& connection) {
  auto reply = connection.xinput()
                   .XIGetSelectedEvents({connection.default_root()})
                   .Sync();
  if (!reply) {
    return false;
  }
  for (const auto& mask : reply->masks) {
    if (mask.deviceid != x11::Input::DeviceId::AllMaster) {
      continue;
    }
    for (auto bits : mask.mask) {
      if (static_cast<uint32_t>(bits) &
          static_cast<uint32_t>(x11::Input::XIEventMask::RawButtonRelease)) {
        return true;
      }
    }
  }
  return false;
}

class X11WmMoveLoopTest : public testing::Test {
 public:
  void SetUp() override {
    connection_ = x11::Connection::Get();
    event_source_ = std::make_unique<X11EventSource>(connection_);
    window_ = connection_->GenerateId<x11::Window>();
    connection_->CreateWindow({
        .wid = window_,
        .parent = connection_->default_root(),
        .width = 100,
        .height = 100,
    });
  }

  void TearDown() override {
    connection_->DestroyWindow({window_});
    connection_->Sync();
  }

 protected:
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::MainThreadType::UI};
  raw_ptr<x11::Connection> connection_ = nullptr;
  std::unique_ptr<X11EventSource> event_source_;
  x11::Window window_ = x11::Window::None;
};

}  // namespace

// The move loop must not block when the button that started the drag has
// already been released, and must clean up its raw event selection.
TEST_F(X11WmMoveLoopTest, ReturnsImmediatelyWithoutButtonPressed) {
  for (bool window_mapped : {true, false}) {
    X11WmMoveLoop move_loop(*connection_, connection_->default_root(), window_);
    EXPECT_TRUE(move_loop.Run(gfx::Vector2d(10, 10), window_mapped));
    EXPECT_FALSE(IsRawButtonReleaseSelected(*connection_));
  }
}

}  // namespace ui
