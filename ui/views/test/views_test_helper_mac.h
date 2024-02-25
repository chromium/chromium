// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_HELPER_MAC_H_
#define UI_VIEWS_TEST_VIEWS_TEST_HELPER_MAC_H_

#include <memory>
#include <optional>

#include "ui/base/test/scoped_fake_full_keyboard_access.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/screen.h"
#include "ui/views/test/views_test_helper.h"

namespace ui::test {
class ScopedFakeNSWindowFocus;
class ScopedFakeNSWindowFullscreen;
}  // namespace ui::test

namespace views {

class ViewsTestHelperMac : public ViewsTestHelper {
 public:
  ViewsTestHelperMac();
  ViewsTestHelperMac(const ViewsTestHelperMac&) = delete;
  ViewsTestHelperMac& operator=(const ViewsTestHelperMac&) = delete;
  ~ViewsTestHelperMac() override;

  // ViewsTestHelper:
  void SetUpTestViewsDelegate(
      TestViewsDelegate* delegate,
      std::optional<ViewsDelegate::NativeWidgetFactory> factory) override;

 private:
  ui::TestContextFactories context_factories_{false};

  // Disable animations during tests.
  ui::ScopedAnimationDurationScaleMode zero_duration_mode_{
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION};

  // When using desktop widgets on Mac, window activation is asynchronous
  // because the window server is involved. A window may also be deactivated by
  // a test running in parallel, making it flaky. In non-interactive/sharded
  // tests, |faked_focus_| is initialized, permitting a unit test to "fake" this
  // activation, causing it to be synchronous and per-process instead.
  std::unique_ptr<ui::test::ScopedFakeNSWindowFocus> faked_focus_;

  // Toggling fullscreen mode on Mac can be flaky for tests run in parallel
  // because only one window may be animating into or out of fullscreen at a
  // time. In non-interactive/sharded tests, |faked_fullscreen_| is initialized,
  // permitting a unit test to 'fake' toggling fullscreen mode.
  std::unique_ptr<ui::test::ScopedFakeNSWindowFullscreen> faked_fullscreen_;

  // Enable fake full keyboard access by default, so that tests don't depend on
  // system setting of the test machine. Also, this helps to make tests on Mac
  // more consistent with other platforms, where most views are focusable by
  // default.
  ui::test::ScopedFakeFullKeyboardAccess faked_full_keyboard_access_;

  display::ScopedNativeScreen screen_;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_HELPER_MAC_H_
