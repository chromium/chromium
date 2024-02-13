// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_SCOPED_VIEWS_TEST_HELPER_H_
#define UI_VIEWS_TEST_SCOPED_VIEWS_TEST_HELPER_H_

#include <memory>
#include <optional>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_helper.h"
#include "ui/views/views_delegate.h"

namespace views {

class Widget;

// Creates a ViewsTestHelper that is destroyed automatically. Acts like
// ViewsTestBase but allows a test harness to use a different base class, or
// make use of a BrowserTaskEnvironment, rather than the MessageLoop provided
// by ViewsTestBase.
class ScopedViewsTestHelper {
 public:
  // Initialize with the given TestViewsDelegate instance.
  explicit ScopedViewsTestHelper(
      std::unique_ptr<TestViewsDelegate> test_views_delegate = nullptr,
      std::optional<ViewsDelegate::NativeWidgetFactory> factory = std::nullopt);
  ScopedViewsTestHelper(const ScopedViewsTestHelper&) = delete;
  ScopedViewsTestHelper& operator=(const ScopedViewsTestHelper&) = delete;
  ~ScopedViewsTestHelper();

  // Returns the context for creating new windows. In Aura builds, this will be
  // the RootWindow. Everywhere else, null.
  gfx::NativeWindow GetContext();

  // Simulate an OS-level destruction of the native window held by non-desktop
  // |widget|.
  void SimulateNativeDestroy(Widget* widget);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  // Simulate an OS-level destruction of the native window held by desktop
  // |widget|.
  void SimulateDesktopNativeDestroy(Widget* widget);
#endif

  TestViewsDelegate* test_views_delegate() {
    return test_views_delegate_.get();
  }

 private:
  std::unique_ptr<ViewsTestHelper> test_helper_ = ViewsTestHelper::Create();
  std::unique_ptr<TestViewsDelegate> test_views_delegate_;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_SCOPED_VIEWS_TEST_HELPER_H_
