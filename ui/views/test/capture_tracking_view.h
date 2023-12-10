// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_CAPTURE_TRACKING_VIEW_H_
#define UI_VIEWS_TEST_CAPTURE_TRACKING_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views::test {

// Used to track OnMousePressed() and OnMouseCaptureLost().
class CaptureTrackingView : public views::View {
  METADATA_HEADER(CaptureTrackingView, views::View)

 public:
  CaptureTrackingView();

  CaptureTrackingView(const CaptureTrackingView&) = delete;
  CaptureTrackingView& operator=(const CaptureTrackingView&) = delete;

  ~CaptureTrackingView() override;

  // Returns true if OnMousePressed() has been invoked.
  bool got_press() const { return got_press_; }

  // Returns true if OnMouseCaptureLost() has been invoked.
  bool got_capture_lost() const { return got_capture_lost_; }

  void reset() { got_press_ = got_capture_lost_ = false; }

  // Overridden from views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;

 private:
  // See description above getters.
  bool got_press_ = false;
  bool got_capture_lost_ = false;
};

}  // namespace views::test

#endif  // UI_VIEWS_TEST_CAPTURE_TRACKING_VIEW_H_
