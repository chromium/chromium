// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper.h"

#include "ui/views/test/test_views_delegate.h"

namespace views {

std::unique_ptr<TestViewsDelegate>
ViewsTestHelper::GetFallbackTestViewsDelegate() {
  return std::make_unique<TestViewsDelegate>();
}

void ViewsTestHelper::SetUpTestViewsDelegate(
    TestViewsDelegate* delegate,
    std::optional<ViewsDelegate::NativeWidgetFactory> factory) {
  if (factory.has_value())
    delegate->set_native_widget_factory(factory.value());
}

void ViewsTestHelper::SetUp() {}

gfx::NativeWindow ViewsTestHelper::GetContext() {
  return nullptr;
}

}  // namespace views
