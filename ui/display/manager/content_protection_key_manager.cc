// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/content_protection_key_manager.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/display_features.h"
#include "ui/display/manager/util/display_manager_util.h"

namespace display {

namespace {

// Length of the key as expected to come from the server.
// This is the same length that the kernel also expects.
constexpr size_t kHdcpKeySize = 285;

display::DisplaySnapshot* GetDisplayWithIdIfHdcpCapableAndKeyNeeded(
    const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
        displays_states,
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
    const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>&
        displays_states,
    int64_t display_id,
    KeySetCallback on_key_set) {
  DCHECK(!on_key_set.is_null());

  // TODO(markyacoub): Remove this flag once the feature is fully launched.
  if (!features::IsHdcpKeyProvisioningRequired()) {
    std::move(on_key_set).Run(false);
    return;
  }

  if (!GetDisplayWithIdIfHdcpCapableAndKeyNeeded(displays_states, display_id)) {
    std::move(on_key_set).Run(false);
    return;
  }

  pending_display_callbacks_[display_id] = std::move(on_key_set);

  // If we already learnt that we need a key and already fetched the key, go
  // ahead and inject it into the kernel.
  if (!cached_provisioned_key_.empty()) {
    InjectKeyToKernel(display_id);
    return;
  }

  // If there are no pending displays nor we have the key, it means we haven't
  // fetched the key from the server yet.
  if (displays_pending_set_key_.empty()) {
    displays_pending_set_key_.insert(display_id);
    FetchKeyFromServer();
    // If the list isn't empty, it means we're in the process of fetching the
    // key from the server already. Just add the pending display to the list and
    // we'll set it later when the key is fetched.
  } else {
    displays_pending_set_key_.insert(display_id);
  }
}

void ContentProtectionKeyManager::FetchKeyFromServer() {
  DCHECK(!provisioned_key_request_.is_null());
  provisioned_key_request_.Run(
      base::BindOnce(&ContentProtectionKeyManager::OnKeyFetchedFromServer,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContentProtectionKeyManager::OnKeyFetchedFromServer(
    const std::string& key) {
  if (key.size()) {
    // This is the size of the key that we expect from the server as of now.
    DCHECK_EQ(key.size(), kHdcpKeySize);
    cached_provisioned_key_ = key;
    for (int64_t display_id : displays_pending_set_key_) {
      InjectKeyToKernel(display_id);
    }
  } else {
    LOG(ERROR) << "Fetched an empty HDCP key from widevine server";
    for (int64_t display_id : displays_pending_set_key_) {
      TriggerPendingCallbacks(display_id, false);
    }
  }
  displays_pending_set_key_.clear();
}

void ContentProtectionKeyManager::InjectKeyToKernel(int64_t display_id) {
  DCHECK(native_display_delegate_);
  native_display_delegate_->SetHdcpKeyProp(
      display_id, cached_provisioned_key_,
      base::BindOnce(&ContentProtectionKeyManager::OnKeyInjectedToKernel,
                     weak_ptr_factory_.GetWeakPtr(), display_id));
}

void ContentProtectionKeyManager::OnKeyInjectedToKernel(int64_t display_id,
                                                        bool success) {
  LOG_IF(ERROR, !success) << "Failed to Inject the HDCP Key to Display #"
                          << display_id;
  TriggerPendingCallbacks(display_id, success);
}

void ContentProtectionKeyManager::TriggerPendingCallbacks(int64_t display_id,
                                                          bool is_key_set) {
  CHECK(base::Contains(pending_display_callbacks_, display_id));
  KeySetCallback callback = std::move(pending_display_callbacks_[display_id]);
  DCHECK(!callback.is_null());
  std::move(callback).Run(is_key_set);

  pending_display_callbacks_.erase(display_id);
}

}  // namespace display
