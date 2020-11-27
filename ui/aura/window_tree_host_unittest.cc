// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/test_event_rewriter.h"
#include "ui/platform_window/stub/stub_window.h"

namespace aura {

using WindowTreeHostTest = test::AuraTestBase;

TEST_F(WindowTreeHostTest, DPIWindowSize) {
  constexpr gfx::Rect starting_bounds(
      aura::test::AuraTestHelper::kDefaultHostSize);

  EXPECT_EQ(starting_bounds.size(), host()->compositor()->size());
  EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
  EXPECT_EQ(starting_bounds, root_window()->bounds());

  test_screen()->SetDeviceScaleFactor(1.5f);
  EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
  // Size should be rounded up after scaling.
  EXPECT_EQ(gfx::Rect(0, 0, 534, 400), root_window()->bounds());

  gfx::Transform transform;
  transform.Translate(0, -1.1f);
  host()->SetRootTransform(transform);
  EXPECT_EQ(gfx::Rect(0, 1, 534, 401), root_window()->bounds());

  EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
  EXPECT_EQ(gfx::Rect(0, 1, 534, 401), root_window()->bounds());
}

TEST_F(WindowTreeHostTest,
       ShouldHaveExactRootWindowBoundsWithDisplayRotation1xScale) {
  test_screen()->SetDeviceScaleFactor(1.f);

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_0);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_0);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(gfx::Rect(400, 300), host()->window()->bounds());

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_90);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(300, 400));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 300, 400));
  EXPECT_EQ(gfx::Rect(300, 400), host()->window()->bounds());

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_180);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_180);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(gfx::Rect(400, 300), host()->window()->bounds());

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_270);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_270);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(300, 400));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 300, 400));
  EXPECT_EQ(gfx::Rect(300, 400), host()->window()->bounds());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WindowTreeHostTest, HoldPointerMovesOnChildResizing) {
  aura::WindowEventDispatcher* dispatcher = host()->dispatcher();

  aura::test::WindowEventDispatcherTestApi dispatcher_api(dispatcher);

  EXPECT_FALSE(dispatcher_api.HoldingPointerMoves());

  // Signal to the ui::Compositor that a child is resizing. This will
  // immediately trigger input throttling.
  host()->compositor()->OnChildResizing();

  // Pointer moves should be throttled until the next commit. This has the
  // effect of prioritizing the resize event above other operations in aura.
  EXPECT_TRUE(dispatcher_api.HoldingPointerMoves());

  // Wait for a CompositorFrame to be activated.
  ui::DrawWaiterForTest::WaitForCompositingEnded(host()->compositor());

  // Pointer moves should be routed normally after commit.
  EXPECT_FALSE(dispatcher_api.HoldingPointerMoves());
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests if scale factor changes take effect. Previously a scale factor change
// wouldn't take effect without a bounds change. For context see
// https://crbug.com/1087626
TEST_F(WindowTreeHostTest, ShouldHandleTextScale) {
  constexpr gfx::Rect starting_bounds(
      aura::test::AuraTestHelper::kDefaultHostSize);
  auto asserter = [&](float test_scale_factor) {
    test_screen()->SetDeviceScaleFactor(test_scale_factor, false);

    EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
    // Size should be rounded up after scaling.
    EXPECT_EQ(
        gfx::ScaleToEnclosingRect(starting_bounds, 1.0f / test_scale_factor),
        root_window()->bounds());
    EXPECT_EQ(test_scale_factor, host()->device_scale_factor());
  };

  asserter(1.0f);
  asserter(1.05f);
  asserter(1.5f);
}
#endif

TEST_F(WindowTreeHostTest, NoRewritesPostIME) {
  ui::test::TestEventRewriter event_rewriter;
  host()->AddEventRewriter(&event_rewriter);

  ui::KeyEvent key_event('A', ui::VKEY_A, ui::DomCode::NONE, 0);
  ui::EventDispatchDetails details =
      host()->GetInputMethod()->DispatchKeyEvent(&key_event);
  ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);
  EXPECT_EQ(0, event_rewriter.events_seen());

  host()->RemoveEventRewriter(&event_rewriter);
}

class TestWindow : public ui::StubWindow {
 public:
  explicit TestWindow(ui::PlatformWindowDelegate* delegate)
      : StubWindow(delegate, false, gfx::Rect(400, 600)) {}
  ~TestWindow() override {}

 private:
  // ui::StubWindow
  void Close() override {
    // It is possible for the window to receive capture-change messages during
    // destruction, for example on Windows (see crbug.com/770670).
    delegate()->OnLostCapture();
  }

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

class TestWindowTreeHost : public WindowTreeHostPlatform {
 public:
  TestWindowTreeHost() {
    SetPlatformWindow(std::make_unique<TestWindow>(this));
    CreateCompositor();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeHost);
};

TEST_F(WindowTreeHostTest, LostCaptureDuringTearDown) {
  TestWindowTreeHost host;
}

}  // namespace aura
