// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_CONFIGURABLE_TEST_NATIVE_FRAME_VIEW_H_
#define UI_VIEWS_TEST_CONFIGURABLE_TEST_NATIVE_FRAME_VIEW_H_

#include <optional>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

class Widget;

namespace test {

// A test-only FrameView that allows for configuring specific
// behaviors needed by tests, such as returning a custom minimum size or a
// custom hit-test result.
class ConfigurableTestNativeFrameView : public NativeFrameView {
  METADATA_HEADER(ConfigurableTestNativeFrameView, NativeFrameView)

 public:
  explicit ConfigurableTestNativeFrameView(Widget* widget);
  ~ConfigurableTestNativeFrameView() override;

  ConfigurableTestNativeFrameView(const ConfigurableTestNativeFrameView&) =
      delete;
  ConfigurableTestNativeFrameView& operator=(
      const ConfigurableTestNativeFrameView&) = delete;

  void set_minimum_size(const gfx::Size& size) { minimum_size_ = size; }
  void set_hit_test_result(int result) { hit_test_result_ = result; }
  bool fullscreen_layout_called() { return fullscreen_layout_caled_; }
  void set_client_view_margin(const gfx::Size& margin) {
    client_view_margin_ = margin;
  }

  // Views
  gfx::Size GetMinimumSize() const override;

  // NativeFrameView
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void Layout(PassKey) override;

 private:
  std::optional<gfx::Size> minimum_size_;
  std::optional<int> hit_test_result_;
  bool fullscreen_layout_caled_ = false;
  std::optional<gfx::Size> client_view_margin_;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_CONFIGURABLE_TEST_NATIVE_FRAME_VIEW_H_
