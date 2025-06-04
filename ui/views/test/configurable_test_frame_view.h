// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_CONFIGURABLE_TEST_FRAME_VIEW_H_
#define UI_VIEWS_TEST_CONFIGURABLE_TEST_FRAME_VIEW_H_

#include <optional>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

class Widget;

namespace test {

// A test-only NonClientFrameView that allows for configuring specific
// behaviors needed by tests, such as returning a custom minimum size or a
// custom hit-test result.
class ConfigurableTestFrameView : public NativeFrameView {
  METADATA_HEADER(ConfigurableTestFrameView, NativeFrameView)
 public:
  explicit ConfigurableTestFrameView(Widget* frame);
  ~ConfigurableTestFrameView() override;

  ConfigurableTestFrameView(const ConfigurableTestFrameView&) = delete;
  ConfigurableTestFrameView& operator=(const ConfigurableTestFrameView&) =
      delete;

  void SetMinimumSize(const gfx::Size& size) { minimum_size_ = size; }
  void SetHitTestResult(int result) { hit_test_result_ = result; }

  // Views
  gfx::Size GetMinimumSize() const override;

  // NativeFrameView
  int NonClientHitTest(const gfx::Point& point) override;

 private:
  std::optional<gfx::Size> minimum_size_;
  std::optional<int> hit_test_result_;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_CONFIGURABLE_TEST_FRAME_VIEW_H_
