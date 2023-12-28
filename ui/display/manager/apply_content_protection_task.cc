// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/apply_content_protection_task.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/util/display_manager_util.h"
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

    if (protection_mask & kContentProtectionMethodHdcpAll)
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
        *display,
        base::BindOnce(&ApplyContentProtectionTask::OnGetHDCPState,
                       weak_ptr_factory_.GetWeakPtr(), display->display_id()));
  }
}

void ApplyContentProtectionTask::OnGetHDCPState(
    int64_t display_id,
    bool success,
    HDCPState state,
    ContentProtectionMethod protection_method) {
  success_ &= success;

  bool hdcp_enabled = state != HDCP_STATE_UNDESIRED;
  uint32_t desired_hdcp_protections =
      GetDesiredProtectionMask(display_id) & kContentProtectionMethodHdcpAll;
  // Remove Type 0 from the mask if Type 1 is there.
  if (desired_hdcp_protections & CONTENT_PROTECTION_METHOD_HDCP_TYPE_1)
    desired_hdcp_protections &= ~CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;

  if (hdcp_enabled != !!desired_hdcp_protections ||
      desired_hdcp_protections != protection_method) {
    ContentProtectionMethod new_method;
    if (!desired_hdcp_protections)
      new_method = CONTENT_PROTECTION_METHOD_NONE;
    else if (desired_hdcp_protections & CONTENT_PROTECTION_METHOD_HDCP_TYPE_1)
      new_method = CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
    else
      new_method = CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;

    hdcp_requests_.emplace_back(
        display_id,
        desired_hdcp_protections ? HDCP_STATE_DESIRED : HDCP_STATE_UNDESIRED,
        new_method);
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

  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> displays =
      layout_manager_->GetDisplayStates();
  std::vector<std::tuple<DisplaySnapshot*, HDCPState, ContentProtectionMethod>>
      hdcped_displays;
  // Lookup the displays again since display configuration may have changed.
  for (const auto& request : hdcp_requests_) {
    auto it = base::ranges::find(displays, request.display_id,
                                 &DisplaySnapshot::display_id);
    if (it == displays.end()) {
      std::move(callback_).Run(Status::FAILURE);
      return;
    }

    hdcped_displays.emplace_back(*it, request.state, request.protection_method);
  }

  // In synchronous callback execution this task can be deleted from the last
  // invocation of SetHDCPState(), thus the for-loop should not iterate over
  // object specific state (eg: |hdcp_requests_|).
  for (const auto& tuple : hdcped_displays) {
    native_display_delegate_->SetHDCPState(
        *std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple),
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
