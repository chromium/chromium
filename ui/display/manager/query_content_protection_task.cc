// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/query_content_protection_task.h"

#include <utility>

#include "base/functional/bind.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

QueryContentProtectionTask::QueryContentProtectionTask(
    DisplayLayoutManager* layout_manager,
    NativeDisplayDelegate* native_display_delegate,
    int64_t display_id,
    ResponseCallback callback)
    : layout_manager_(layout_manager),
      native_display_delegate_(native_display_delegate),
      display_id_(display_id),
      callback_(std::move(callback)) {}

QueryContentProtectionTask::~QueryContentProtectionTask() {
  if (callback_) {
    std::move(callback_).Run(Status::KILLED, DISPLAY_CONNECTION_TYPE_NONE,
                             CONTENT_PROTECTION_METHOD_NONE);
  }
}

void QueryContentProtectionTask::Run() {
  std::vector<DisplaySnapshot*> hdcp_capable_displays;
  for (DisplaySnapshot* display : layout_manager_->GetDisplayStates()) {
    // Query all displays in mirroring mode. Otherwise, query the given display,
    // which must exist because tasks are killed on display reconfiguration.
    if (!layout_manager_->IsMirroring() && display->display_id() != display_id_)
      continue;

    connection_mask_ |= display->type();

    uint32_t protection_mask;
    if (!GetContentProtectionMethods(display->type(), &protection_mask)) {
      std::move(callback_).Run(Status::FAILURE, DISPLAY_CONNECTION_TYPE_UNKNOWN,
                               CONTENT_PROTECTION_METHOD_NONE);
      return;
    }

    // Collect displays to be queried based on HDCP capability. For unprotected
    // displays not inherently secure through an internal connection, record the
    // existence of an unsecure display to report no protection for all displays
    // in mirroring mode.
    if (protection_mask & kContentProtectionMethodHdcpAll)
      hdcp_capable_displays.push_back(display);
    else if (display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL)
      no_protection_mask_ |= kContentProtectionMethodHdcpAll;
  }

  pending_requests_ = hdcp_capable_displays.size();
  if (pending_requests_ == 0) {
    std::move(callback_).Run(Status::SUCCESS, connection_mask_,
                             CONTENT_PROTECTION_METHOD_NONE);
    return;
  }

  for (DisplaySnapshot* display : hdcp_capable_displays) {
    native_display_delegate_->GetHDCPState(
        *display, base::BindOnce(&QueryContentProtectionTask::OnGetHDCPState,
                                 weak_ptr_factory_.GetWeakPtr()));
  }
}

void QueryContentProtectionTask::OnGetHDCPState(
    bool success,
    HDCPState state,
    ContentProtectionMethod protection_method) {
  success_ &= success;

  if (state == HDCP_STATE_ENABLED)
    protection_mask_ |= protection_method;
  else
    no_protection_mask_ |= kContentProtectionMethodHdcpAll;

  pending_requests_--;
  // Wait for all the requests to finish before invoking the callback.
  if (pending_requests_ != 0)
    return;

  protection_mask_ &= ~no_protection_mask_;

  // If HDCP Type 1 and Type 0 are in the protection mask, then remove Type 1
  // from the mask since we want to reflect the overall output security. If only
  // Type 1 is in the protection mask, then also add Type 0 to the mask so we
  // properly support clients that were written before Type 1 was added.
  if ((protection_mask_ & kContentProtectionMethodHdcpAll) ==
      kContentProtectionMethodHdcpAll) {
    protection_mask_ &= ~CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
  } else if (protection_mask_ & CONTENT_PROTECTION_METHOD_HDCP_TYPE_1) {
    protection_mask_ |= CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
  }
  std::move(callback_).Run(success_ ? Status::SUCCESS : Status::FAILURE,
                           connection_mask_, protection_mask_);
}

}  // namespace display
