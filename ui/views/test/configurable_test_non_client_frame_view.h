// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_CONFIGURABLE_TEST_NON_CLIENT_FRAME_VIEW_H_
#define UI_VIEWS_TEST_CONFIGURABLE_TEST_NON_CLIENT_FRAME_VIEW_H_

#include <utility>

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/non_client_view.h"

namespace views::test {

// A test-only NonClientFrameView for custom frames (not based on
// NativeFrameView) that allows for configuring behaviors like window shapring
// and hit-testing via callbacks.
class ConfigurableTestNonClientFrameView : public NonClientFrameView {
  METADATA_HEADER(ConfigurableTestNonClientFrameView, NonClientFrameView)

 public:
  using WindowMaskCallback =
      base::RepeatingCallback<void(const gfx::Size&, SkPath*)>;
  using HitTestCallback = base::RepeatingCallback<int(const gfx::Point&)>;

  ConfigurableTestNonClientFrameView();
  ConfigurableTestNonClientFrameView(
      const ConfigurableTestNonClientFrameView&) = delete;
  ConfigurableTestNonClientFrameView& operator=(
      const ConfigurableTestNonClientFrameView&) = delete;
  ~ConfigurableTestNonClientFrameView() override;

  // Configuration
  void SetWindowMaskCallback(WindowMaskCallback callback) {
    window_mask_callback_ = std::move(callback);
  }

  void SetHitTestCallback(HitTestCallback callback) {
    hit_test_callback_ = std::move(callback);
  }

 private:
  // NonClientFrameView
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // When run this callback should set a custom window mask on the frame.
  WindowMaskCallback window_mask_callback_;

  // When run this callback should perform a custom hit test on the frame.
  HitTestCallback hit_test_callback_;
};

}  // namespace views::test

#endif  // UI_VIEWS_TEST_CONFIGURABLE_TEST_NON_CLIENT_FRAME_VIEW_H_
