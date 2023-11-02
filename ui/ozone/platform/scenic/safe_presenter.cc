// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/safe_presenter.h"

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>

#include "base/check.h"

namespace ui {

SafePresenter::SafePresenter(scenic::Session* session) : session_(session) {
  DCHECK(session_);

  session_->set_on_frame_presented_handler(
      fit::bind_member(this, &SafePresenter::OnFramePresented));
}

SafePresenter::~SafePresenter() {
  // Clear the OnFramePresented callback, in case the `session_` outlives
  // SafePresenter.
  session_->set_on_frame_presented_handler({});
}

void SafePresenter::QueuePresent() {
  // Present to Scenic immediately, if we can.
  if (presents_allowed_) {
    QueuePresentHelper();
    return;
  }

  // We cannot present immediately, so queue for later.
  present_queued_ = true;
}

void SafePresenter::QueuePresentHelper() {
  DCHECK(session_);
  DCHECK(presents_allowed_);

  presents_allowed_ = false;
  present_queued_ = false;
  session_->Present2(/*requested_presentation_time=*/0,
                     /*requested_prediction_span=*/0, [](auto) {});
}

void SafePresenter::OnFramePresented(
    fuchsia::scenic::scheduling::FramePresentedInfo info) {
  presents_allowed_ = info.num_presents_allowed > 0;
  // Since we only have one Present2 call in progress at once, this must
  // be true.
  DCHECK(presents_allowed_);

  if (present_queued_ && presents_allowed_) {
    QueuePresentHelper();
  }
}

}  // namespace ui
