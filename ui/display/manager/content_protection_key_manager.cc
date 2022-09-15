// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/content_protection_key_manager.h"

#include "ui/display/display_features.h"
#include "ui/display/manager/display_manager_util.h"

namespace {
std::vector<display::DisplaySnapshot*> GetHdcpCapableDisplays(
    const std::vector<display::DisplaySnapshot*>& displays_states) {
  std::vector<display::DisplaySnapshot*> hdcp_capable_displays;
  for (display::DisplaySnapshot* display : displays_states) {
    uint32_t protection_mask;
    if (GetContentProtectionMethods(display->type(), &protection_mask) &&
        protection_mask & display::kContentProtectionMethodHdcpAll) {
      hdcp_capable_displays.push_back(display);
    }
  }
  return hdcp_capable_displays;
}
}  // namespace

namespace display {

ContentProtectionKeyManager::ContentProtectionKeyManager() = default;
ContentProtectionKeyManager::~ContentProtectionKeyManager() = default;

void ContentProtectionKeyManager::SetKeyIfRequired(
    const std::vector<DisplaySnapshot*>& displays_states,
    NativeDisplayDelegate* native_display_delegate,
    KeySetCallback on_key_set) {
  // TODO(markyacoub): Query the hdcp key and inject it if needed to
  // replace the feature flag.
  if (!features::IsHdcpKeyProvisioningRequired()) {
    std::move(on_key_set).Run();
    return;
  }

  if (key_fetching_status_ == KeyFetchingStatus::kFetchingNotRequired ||
      provisioned_key_request_.is_null()) {
    std::move(on_key_set).Run();
    return;
  }

  // Check if any of the displays support HDCP.
  std::vector<DisplaySnapshot*> hdcp_capable_displays =
      GetHdcpCapableDisplays(displays_states);
  pending_displays_to_configure_ = hdcp_capable_displays.size();
  if (hdcp_capable_displays.empty()) {
    std::move(on_key_set).Run();
    return;
  }

  // If we already learnt that we need a key and already fetched the key, go
  // ahead and inject it into the kernel.
  if (!cached_provisioned_key_.empty()) {
    for (DisplaySnapshot* display : hdcp_capable_displays) {
      // TODO(markyacoub): inject key to kernel.
      (void)display;
    }
    return;
  }

  // TODO(markyacoub): Implement querying and injecting.
  std::move(on_key_set).Run();
}

}  // namespace display
