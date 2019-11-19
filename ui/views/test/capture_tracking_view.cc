// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/capture_tracking_view.h"

namespace views {
namespace test {

CaptureTrackingView::CaptureTrackingView() = default;

CaptureTrackingView::~CaptureTrackingView() = default;

bool CaptureTrackingView::OnMousePressed(const ui::MouseEvent& event) {
  got_press_ = true;
  return true;
}

void CaptureTrackingView::OnMouseCaptureLost() {
  got_capture_lost_ = true;
}

}  // namespace test
}  // namespace views
