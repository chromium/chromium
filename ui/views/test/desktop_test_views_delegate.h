// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_DESKTOP_TEST_VIEWS_DELEGATE_H_
#define UI_VIEWS_TEST_DESKTOP_TEST_VIEWS_DELEGATE_H_

#include "ui/views/test/test_views_delegate.h"

namespace views {

// Most aura test code is written assuming a single RootWindow view, however,
// at higher levels like content_browsertests and
// views_examples_with_content_exe, we must use the Desktop variants.
class DesktopTestViewsDelegate : public TestViewsDelegate {
 public:
  DesktopTestViewsDelegate();

  DesktopTestViewsDelegate(const DesktopTestViewsDelegate&) = delete;
  DesktopTestViewsDelegate& operator=(const DesktopTestViewsDelegate&) = delete;

  ~DesktopTestViewsDelegate() override;

  // Overridden from ViewsDelegate:
  void OnBeforeWidgetInit(Widget::InitParams* params,
                          internal::NativeWidgetDelegate* delegate) override;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_DESKTOP_TEST_VIEWS_DELEGATE_H_
