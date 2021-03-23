// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/platform_window/stub/stub_window.h"

namespace aura {
namespace {

using WindowTreeHostPlatformTest = test::AuraTestBase;

// Trivial WindowTreeHostPlatform implementation that installs a StubWindow as
// the PlatformWindow.
class TestWindowTreeHost : public WindowTreeHostPlatform {
 public:
  TestWindowTreeHost() {
    SetPlatformWindow(std::make_unique<ui::StubWindow>(this));
    CreateCompositor();
  }

  ui::PlatformWindow* platform_window() {
    return WindowTreeHostPlatform::platform_window();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeHost);
};

// WindowTreeHostObserver that tracks calls to
// OnHostWill/DidProcessBoundsChange. Additionally, this triggers a bounds
// change from within OnHostResized(). Such a scenario happens in production
// code.
class TestWindowTreeHostObserver : public WindowTreeHostObserver {
 public:
  TestWindowTreeHostObserver(WindowTreeHostPlatform* host,
                             ui::PlatformWindow* platform_window)
      : host_(host), platform_window_(platform_window) {
    host_->AddObserver(this);
  }
  ~TestWindowTreeHostObserver() override { host_->RemoveObserver(this); }

  int on_host_did_process_bounds_change_count() const {
    return on_host_did_process_bounds_change_count_;
  }

  int on_host_will_process_bounds_change_count() const {
    return on_host_will_process_bounds_change_count_;
  }

  // WindowTreeHostObserver:
  void OnHostResized(WindowTreeHost* host) override {
    if (!should_change_bounds_in_on_resized_)
      return;

    should_change_bounds_in_on_resized_ = false;
    gfx::Rect bounds = platform_window_->GetBounds();
    bounds.set_x(bounds.x() + 1);
    host_->SetBoundsInPixels(bounds);
  }
  void OnHostWillProcessBoundsChange(WindowTreeHost* host) override {
    ++on_host_will_process_bounds_change_count_;
  }
  void OnHostDidProcessBoundsChange(WindowTreeHost* host) override {
    ++on_host_did_process_bounds_change_count_;
  }

 private:
  WindowTreeHostPlatform* host_;
  ui::PlatformWindow* platform_window_;
  bool should_change_bounds_in_on_resized_ = true;
  int on_host_will_process_bounds_change_count_ = 0;
  int on_host_did_process_bounds_change_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeHostObserver);
};

// Regression test for https://crbug.com/958449
TEST_F(WindowTreeHostPlatformTest, HostWillProcessBoundsChangeRecursion) {
  TestWindowTreeHost host;
  TestWindowTreeHostObserver observer(&host, host.platform_window());
  // This call triggers a recursive bounds change. That is, this results in
  // WindowTreePlatform::OnBoundsChanged() indirectly calling back into
  // WindowTreePlatform::OnBoundsChanged(). In such a scenario the observer
  // should be notified only once (see comment in
  // WindowTreeHostPlatform::OnBoundsChanged() for details).
  host.SetBoundsInPixels(gfx::Rect(1, 2, 3, 4));
  EXPECT_EQ(1, observer.on_host_did_process_bounds_change_count());
  EXPECT_EQ(1, observer.on_host_will_process_bounds_change_count());
}

// Deletes WindowTreeHostPlatform from OnHostMovedInPixels().
class DeleteHostWindowTreeHostObserver : public WindowTreeHostObserver {
 public:
  explicit DeleteHostWindowTreeHostObserver(
      std::unique_ptr<TestWindowTreeHost> host)
      : host_(std::move(host)) {
    host_->AddObserver(this);
  }
  ~DeleteHostWindowTreeHostObserver() override = default;

  TestWindowTreeHost* host() { return host_.get(); }

  // WindowTreeHostObserver:
  void OnHostMovedInPixels(WindowTreeHost* host,
                           const gfx::Point& new_origin_in_pixels) override {
    host_->RemoveObserver(this);
    host_.reset();
  }

 private:
  std::unique_ptr<TestWindowTreeHost> host_;

  DISALLOW_COPY_AND_ASSIGN(DeleteHostWindowTreeHostObserver);
};

// Verifies WindowTreeHostPlatform can be safely deleted when calling
// OnHostMovedInPixels().
// Regression test for https://crbug.com/1185482
TEST_F(WindowTreeHostPlatformTest, DeleteHostFromOnHostMovedInPixels) {
  std::unique_ptr<TestWindowTreeHost> host =
      std::make_unique<TestWindowTreeHost>();
  DeleteHostWindowTreeHostObserver observer(std::move(host));
  observer.host()->SetBoundsInPixels(gfx::Rect(1, 2, 3, 4));
  EXPECT_EQ(nullptr, observer.host());
}

}  // namespace
}  // namespace aura
