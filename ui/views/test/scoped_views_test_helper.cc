// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/scoped_views_test_helper.h"

#include <utility>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {

ScopedViewsTestHelper::ScopedViewsTestHelper(
    std::unique_ptr<TestViewsDelegate> test_views_delegate,
    absl::optional<ViewsDelegate::NativeWidgetFactory> factory)
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
#endif

}  // namespace views
