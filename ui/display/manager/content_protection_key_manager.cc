// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/content_protection_key_manager.h"

#include "base/containers/contains.h"
#include "ui/display/display_features.h"
#include "ui/display/manager/display_manager_util.h"

namespace display {

namespace {

display::DisplaySnapshot* GetDisplayWithIdIfHdcpCapableAndKeyNeeded(
    const std::vector<display::DisplaySnapshot*>& displays_states,
    int64_t display_id) {
  for (display::DisplaySnapshot* display : displays_states) {
    if (display->display_id() == display_id) {
      uint32_t protection_mask;
      bool is_hdcp_capable =
          GetContentProtectionMethods(display->type(), &protection_mask) &&
          protection_mask & display::kContentProtectionMethodHdcpAll;
      if (is_hdcp_capable && display->has_content_protection_key()) {
        return display;
      }
      break;
    }
  }

  return nullptr;
}

}  // namespace

ContentProtectionKeyManager::ContentProtectionKeyManager() = default;
ContentProtectionKeyManager::~ContentProtectionKeyManager() = default;

void ContentProtectionKeyManager::SetKeyIfRequired(
    const std::vector<DisplaySnapshot*>& displays_states,
    int64_t display_id,
    KeySetCallback on_key_set) {
  DCHECK(!on_key_set.is_null());

  // TODO(markyacoub): Remove this flag once the feature is fully launched.
  if (!features::IsHdcpKeyProvisioningRequired()) {
    std::move(on_key_set).Run();
    return;
  }

  if (!GetDisplayWithIdIfHdcpCapableAndKeyNeeded(displays_states, display_id)) {
    std::move(on_key_set).Run();
    return;
  }

  pending_display_callbacks_[display_id] = std::move(on_key_set);

  // If we already learnt that we need a key and already fetched the key, go
  // ahead and inject it into the kernel.
  if (!cached_provisioned_key_.empty()) {
    // TODO(markyacoub): Replace and inject key to kernel.
    TriggerPendingCallbacks(display_id);
    return;
  }

  // TODO(markyacoub): fetch the key from the server. For now, just end the
  // process and call the callback when all displays have responded.
  TriggerPendingCallbacks(display_id);
}

void ContentProtectionKeyManager::TriggerPendingCallbacks(int64_t display_id) {
  CHECK(base::Contains(pending_display_callbacks_, display_id));
  KeySetCallback callback = std::move(pending_display_callbacks_[display_id]);
  DCHECK(!callback.is_null());
  std::move(callback).Run();

  pending_display_callbacks_.erase(display_id);
}

}  // namespace display
