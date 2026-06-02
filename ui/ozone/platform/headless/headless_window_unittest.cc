// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
namespace {

// A PlatformWindowDelegate that simulates the production behaviour of
// DesktopWindowTreeHostPlatform: when the platform window calls back into the
// delegate (OnBoundsChanged / OnWindowStateChanged), an observer may
// synchronously call Widget::CloseNow(), which ends up calling
// HeadlessWindow::Close() -> delegate_->OnClosed() ->
// DWTHP::OnClosed() -> SetPlatformWindow(nullptr) -> ~HeadlessWindow().
//
// This delegate models that by resetting its owned unique_ptr<PlatformWindow>
// from inside the chosen callback.
class DestroyingDelegate : public PlatformWindowDelegate {
 public:
  enum class DestroyOn {
    kNone,
    kBoundsChanged,
    kWindowStateChanged,
  };

  explicit DestroyingDelegate(DestroyOn destroy_on) : destroy_on_(destroy_on) {}
  ~DestroyingDelegate() override = default;

  void set_destroy_on(DestroyOn destroy_on) { destroy_on_ = destroy_on; }

  void SetWindow(std::unique_ptr<PlatformWindow> window) {
    window_ = std::move(window);
  }

  // PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange& change) override {
    if (destroy_on_ == DestroyOn::kBoundsChanged) {
      window_.reset();
    }
  }
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override { window_.reset(); }
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override {
    if (destroy_on_ == DestroyOn::kWindowStateChanged) {
      window_.reset();
    }
  }
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnCursorUpdate() override {}
  bool CanMaximize() const override { return true; }
  bool CanFullscreen() const override { return true; }
  gfx::Rect ConvertRectToPixels(const gfx::Rect& rect_in_dip) const override {
    return rect_in_dip;
  }
  gfx::Rect ConvertRectToDIP(const gfx::Rect& rect_in_pixels) const override {
    return rect_in_pixels;
  }

 private:
  DestroyOn destroy_on_ = DestroyOn::kNone;
  std::unique_ptr<PlatformWindow> window_;
};

class HeadlessWindowCrashTest : public ::testing::Test {
 public:
  HeadlessWindowCrashTest() = default;
  ~HeadlessWindowCrashTest() override = default;

 protected:
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  HeadlessWindowManager manager_;
};

TEST_F(HeadlessWindowCrashTest, DestroyViaObserverInSetFullscreen) {
  DestroyingDelegate delegate(DestroyingDelegate::DestroyOn::kBoundsChanged);

  auto window = std::make_unique<HeadlessWindow>(&delegate, &manager_,
                                                 gfx::Rect(0, 0, 100, 100));
  HeadlessWindow* headless_window = window.get();
  delegate.SetWindow(std::move(window));

  headless_window->SetFullscreen(/*fullscreen=*/true,
                                 /*target_display_id=*/-1);
}

TEST_F(HeadlessWindowCrashTest, DestroyViaObserverInMaximize) {
  DestroyingDelegate delegate(
      DestroyingDelegate::DestroyOn::kWindowStateChanged);

  auto window = std::make_unique<HeadlessWindow>(&delegate, &manager_,
                                                 gfx::Rect(0, 0, 100, 100));
  HeadlessWindow* headless_window = window.get();
  delegate.SetWindow(std::move(window));

  headless_window->Maximize();
}

TEST_F(HeadlessWindowCrashTest, DestroyViaObserverInMinimize) {
  DestroyingDelegate delegate(
      DestroyingDelegate::DestroyOn::kWindowStateChanged);

  auto window = std::make_unique<HeadlessWindow>(&delegate, &manager_,
                                                 gfx::Rect(0, 0, 100, 100));
  HeadlessWindow* headless_window = window.get();
  delegate.SetWindow(std::move(window));

  headless_window->Minimize();
}

TEST_F(HeadlessWindowCrashTest, DestroyViaObserverInRestore) {
  DestroyingDelegate delegate(DestroyingDelegate::DestroyOn::kNone);

  auto window = std::make_unique<HeadlessWindow>(&delegate, &manager_,
                                                 gfx::Rect(0, 0, 100, 100));
  HeadlessWindow* headless_window = window.get();
  delegate.SetWindow(std::move(window));

  headless_window->Maximize();

  delegate.set_destroy_on(DestroyingDelegate::DestroyOn::kBoundsChanged);

  headless_window->Restore();
}

}  // namespace
}  // namespace ui
