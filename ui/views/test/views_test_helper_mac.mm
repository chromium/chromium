// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/functional/bind.h"
#include "ui/base/test/scoped_fake_nswindow_focus.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/event_generator_delegate_mac.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
std::unique_ptr<ViewsTestHelper> ViewsTestHelper::Create() {
  return std::make_unique<ViewsTestHelperMac>();
}

ViewsTestHelperMac::ViewsTestHelperMac() {
  // Unbundled applications (those without Info.plist) default to
  // NSApplicationActivationPolicyProhibited, which prohibits the application
  // obtaining key status or activating windows without user interaction.
  NSApp.activationPolicy = NSApplicationActivationPolicyRegular;

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&test::CreateEventGeneratorDelegateMac));

  // Assume that if the methods in the ui_controls.h test header are enabled
  // then the test runner is in a non-sharded mode, and will use "real"
  // activations and fullscreen mode. This allows interactive_ui_tests to test
  // the actual OS window activation and fullscreen codepaths.
  if (!ui_controls::IsUIControlsEnabled()) {
    faked_focus_ = std::make_unique<ui::test::ScopedFakeNSWindowFocus>();
    faked_fullscreen_ =
        std::make_unique<ui::test::ScopedFakeNSWindowFullscreen>();
  }
}

ViewsTestHelperMac::~ViewsTestHelperMac() {
  // Ensure all Widgets are closed explicitly in tests. The Widget may be
  // hosting a Compositor. If that's torn down after the test ContextFactory
  // then a lot of confusing use-after-free errors result. In browser tests,
  // this is handled automatically by views::Widget::CloseAllSecondaryWidgets().
  // Unit tests on Aura may create Widgets owned by a RootWindow that gets torn
  // down, but on Mac we need to be more explicit.
  @autoreleasepool {
    NSArray* native_windows = NSApp.windows;
    for (NSWindow* window : native_windows)
      DCHECK(!Widget::GetWidgetForNativeWindow(window)) << "Widget not closed.";

    ui::test::EventGeneratorDelegate::SetFactoryFunction(
        ui::test::EventGeneratorDelegate::FactoryFunction());
  }
}

void ViewsTestHelperMac::SetUpTestViewsDelegate(
    TestViewsDelegate* delegate,
    std::optional<ViewsDelegate::NativeWidgetFactory> factory) {
  ViewsTestHelper::SetUpTestViewsDelegate(delegate, std::move(factory));
  delegate->set_context_factory(context_factories_.GetContextFactory());
}

}  // namespace views
