// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/scoped_views_test_helper.h"

#include <utility>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#endif

namespace views {

ScopedViewsTestHelper::ScopedViewsTestHelper(
    std::unique_ptr<TestViewsDelegate> test_views_delegate,
    std::optional<ViewsDelegate::NativeWidgetFactory> factory)
    : test_views_delegate_(test_views_delegate
                               ? std::move(test_views_delegate)
                               : test_helper_->GetFallbackTestViewsDelegate()) {
  test_helper_->SetUpTestViewsDelegate(test_views_delegate_.get(),
                                       std::move(factory));
  test_helper_->SetUp();

  // OS clipboard is a global resource, which causes flakiness when unit tests
  // run in parallel. So, use a per-instance test clipboard.
  ui::TestClipboard::CreateForCurrentThread();
}

ScopedViewsTestHelper::~ScopedViewsTestHelper() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

gfx::NativeWindow ScopedViewsTestHelper::GetContext() {
  return test_helper_->GetContext();
}

#if defined(USE_AURA)
void ScopedViewsTestHelper::SimulateNativeDestroy(Widget* widget) {
  delete widget->GetNativeView();
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
void ScopedViewsTestHelper::SimulateDesktopNativeDestroy(Widget* widget) {
  static_cast<DesktopNativeWidgetAura*>(widget->native_widget())
      ->desktop_window_tree_host_for_testing()
      ->Close();
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)
#endif  // defined(USE_AURA)

}  // namespace views
