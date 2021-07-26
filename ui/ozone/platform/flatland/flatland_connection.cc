// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_connection.h"

#include <lib/sys/cpp/component_context.h>

#include "base/check.h"
#include "base/fuchsia/process_context.h"

namespace ui {

FlatlandConnection::FlatlandConnection()
    : flatland_(base::ComponentContextForProcess()
                    ->svc()
                    ->Connect<fuchsia::ui::composition::Flatland>()) {
  flatland_.events().OnFramePresented =
      fit::bind_member(this, &FlatlandConnection::OnFramePresented);
}

FlatlandConnection::~FlatlandConnection() = default;

void FlatlandConnection::QueuePresent() {
  // TODO(crbug.com/1230150): Accumulate calls and wait for OnNextFrameBegin to
  // present after SDK roll instead of presenting immediately.

  // Present to Flatland immediately, if we can.
  if (presents_allowed_) {
    QueuePresentHelper();
    return;
  }

  // We cannot present immediately, so queue for later.
  present_queued_ = true;
}

void FlatlandConnection::QueuePresentHelper() {
  DCHECK(presents_allowed_);

  presents_allowed_ = false;
  present_queued_ = false;

  fuchsia::ui::composition::PresentArgs present_args;
  present_args.set_requested_presentation_time(0);
  present_args.set_acquire_fences({});
  present_args.set_release_fences({});
  present_args.set_unsquashable(false);
  flatland_->Present(std::move(present_args));
}

void FlatlandConnection::OnFramePresented(
    fuchsia::scenic::scheduling::FramePresentedInfo info) {
  presents_allowed_ = info.num_presents_allowed > 0;
  // Since we only have one Present call in progress at once, this must
  // be true.
  DCHECK(presents_allowed_);

  if (present_queued_ && presents_allowed_) {
    QueuePresentHelper();
  }
}

}  // namespace ui
