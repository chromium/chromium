// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_HELPER_H_
#define UI_VIEWS_TEST_VIEWS_TEST_HELPER_H_

#include <memory>
#include <optional>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_delegate.h"

namespace views {

class TestViewsDelegate;

// A helper class owned by tests that performs platform specific initialization
// required for running tests.
class ViewsTestHelper {
 public:
  // Create a platform specific instance.
  static std::unique_ptr<ViewsTestHelper> Create();

  ViewsTestHelper(const ViewsTestHelper&) = delete;
  ViewsTestHelper& operator=(const ViewsTestHelper&) = delete;
  virtual ~ViewsTestHelper() = default;

  // Returns the delegate to use if the test/owner does not create one.
  virtual std::unique_ptr<TestViewsDelegate> GetFallbackTestViewsDelegate();

  // Does any additional necessary setup of the provided |delegate|.
  virtual void SetUpTestViewsDelegate(
      TestViewsDelegate* delegate,
      std::optional<ViewsDelegate::NativeWidgetFactory> factory);

  // Does any additional necessary setup of this object or its members.
  virtual void SetUp();

  // Returns a context window, e.g. the Aura root window.
  virtual gfx::NativeWindow GetContext();

 protected:
  ViewsTestHelper() = default;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_HELPER_H_
