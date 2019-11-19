// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/apply_content_protection_task.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

ApplyContentProtectionTask::ApplyContentProtectionTask(
    DisplayLayoutManager* layout_manager,
    NativeDisplayDelegate* native_display_delegate,
    ContentProtectionManager::ContentProtections requests,
    ResponseCallback callback)
    : layout_manager_(layout_manager),
      native_display_delegate_(native_display_delegate),
      requests_(std::move(requests)),
      callback_(std::move(callback)) {}

ApplyContentProtectionTask::~ApplyContentProtectionTask() {
  if (callback_)
    std::move(callback_).Run(Status::KILLED);
}

void ApplyContentProtectionTask::Run() {
  std::vector<DisplaySnapshot*> hdcp_capable_displays;
  for (DisplaySnapshot* display : layout_manager_->GetDisplayStates()) {
    uint32_t protection_mask;
    if (!GetContentProtectionMethods(display->type(), &protection_mask)) {
      std::move(callback_).Run(Status::FAILURE);
      return;
    }

    if (protection_mask & CONTENT_PROTECTION_METHOD_HDCP)
      hdcp_capable_displays.push_back(display);
  }

  pending_requests_ = hdcp_capable_displays.size();
  if (pending_requests_ == 0) {
    std::move(callback_).Run(Status::SUCCESS);
    return;
  }

  // Need to poll the driver for updates since other applications may have
  // updated the state.
  for (DisplaySnapshot* display : hdcp_capable_displays) {
    native_display_delegate_->GetHDCPState(
        *display, base::BindOnce(&ApplyContentProtectionTask::OnGetHDCPState,
                                 weak_ptr_factory_.GetWeakPtr(), display));
  }
}

void ApplyContentProtectionTask::OnGetHDCPState(DisplaySnapshot* display,
                                                bool success,
                                                HDCPState state) {
  success_ &= success;

  bool hdcp_enabled = state != HDCP_STATE_UNDESIRED;
  bool hdcp_requested = GetDesiredProtectionMask(display->display_id()) &
                        CONTENT_PROTECTION_METHOD_HDCP;

  if (hdcp_enabled != hdcp_requested) {
    hdcp_requests_.emplace_back(
        display, hdcp_requested ? HDCP_STATE_DESIRED : HDCP_STATE_UNDESIRED);
  }

  pending_requests_--;

  // Wait for all the requests before continuing.
  if (pending_requests_ != 0)
    return;

  if (!success_) {
    std::move(callback_).Run(Status::FAILURE);
    return;
  }

  pending_requests_ = hdcp_requests_.size();
  // All the requested changes are the same as the current HDCP state. Nothing
  // to do anymore, just ack the content protection change.
  if (pending_requests_ == 0) {
    std::move(callback_).Run(Status::SUCCESS);
    return;
  }

  for (const auto& pair : hdcp_requests_) {
    native_display_delegate_->SetHDCPState(
        *pair.first, pair.second,
        base::BindOnce(&ApplyContentProtectionTask::OnSetHDCPState,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ApplyContentProtectionTask::OnSetHDCPState(bool success) {
  success_ &= success;
  pending_requests_--;

  if (pending_requests_ == 0)
    std::move(callback_).Run(success_ ? Status::SUCCESS : Status::FAILURE);
}

uint32_t ApplyContentProtectionTask::GetDesiredProtectionMask(
    int64_t display_id) const {
  uint32_t desired_mask = 0;
  // In mirror mode, protection request of all displays need to be fulfilled.
  // In non-mirror mode, only request of client's display needs to be
  // fulfilled.
  if (layout_manager_->IsMirroring()) {
    for (const auto& pair : requests_)
      desired_mask |= pair.second;
  } else {
    auto it = requests_.find(display_id);
    if (it != requests_.end())
      desired_mask = it->second;
  }

  return desired_mask;
}

}  // namespace display
