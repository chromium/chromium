// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_PLATFORM_NATIVE_WIDGET_H_
#define UI_VIEWS_TEST_TEST_PLATFORM_NATIVE_WIDGET_H_

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
namespace internal {
class NativeWidgetDelegate;
}
namespace test {

// NativeWidget implementation that adds the following:
// . capture can be mocked.
// . a boolean is set when the NativeWidget is destroyed.
// Don't create directly, instead go through functions in
// native_widget_factory.h that create the appropriate platform specific
// implementation.
template <typename PlatformNativeWidget>
class TestPlatformNativeWidget : public PlatformNativeWidget {
 public:
  TestPlatformNativeWidget(internal::NativeWidgetDelegate* delegate,
                           bool mock_capture,
                           bool* destroyed)
      : PlatformNativeWidget(delegate),
        mouse_capture_(false),
        mock_capture_(mock_capture),
        destroyed_(destroyed) {}
  TestPlatformNativeWidget(internal::NativeWidgetDelegate* delegate,
                           bool mock_capture,
                           base::OnceClosure destroyed_callback)
      : PlatformNativeWidget(delegate),
        mouse_capture_(false),
        mock_capture_(mock_capture),
        destroyed_(nullptr),
        destroyed_callback_(std::move(destroyed_callback)) {}

  TestPlatformNativeWidget(const TestPlatformNativeWidget&) = delete;
  TestPlatformNativeWidget& operator=(const TestPlatformNativeWidget&) = delete;

  ~TestPlatformNativeWidget() override {
    if (destroyed_)
      *destroyed_ = true;
    if (destroyed_callback_)
      std::move(destroyed_callback_).Run();
  }

  // PlatformNativeWidget:
  void SetCapture() override {
    if (mock_capture_)
      mouse_capture_ = true;
    else
      PlatformNativeWidget::SetCapture();
  }

  void ReleaseCapture() override {
    if (mock_capture_) {
      if (mouse_capture_)
        PlatformNativeWidget::GetWidget()->OnMouseCaptureLost();
      mouse_capture_ = false;
    } else {
      PlatformNativeWidget::ReleaseCapture();
    }
  }

  bool HasCapture() const override {
    return mock_capture_ ? mouse_capture_ : PlatformNativeWidget::HasCapture();
  }

 private:
  bool mouse_capture_;
  const bool mock_capture_;
  raw_ptr<bool> destroyed_;
  base::OnceClosure destroyed_callback_;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_PLATFORM_NATIVE_WIDGET_H_
