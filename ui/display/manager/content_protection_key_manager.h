// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CONTENT_PROTECTION_KEY_MANAGER_H_
#define UI_DISPLAY_MANAGER_CONTENT_PROTECTION_KEY_MANAGER_H_

#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

// ContentProtectionKeyManager is responsible for managing the provisioned HDCP
// key for the displays.
// Some drivers such as msm expose "Content Protection Key" connector prop that
// needs a key to enable HDCP. The key is typically provisioned from a server.

// The Workflow when a key is required:
//  1. Check for a valid key request function.
//  2. Query the "Content Protection Key" connector property to verify if the
//     key is required.
//  3. Check for keys that are cached for the HDCP capable displays, if they
//     don't exist get the provisioned key from the server.
//  4. Inject the key into the kernel by writing the key into the
//     "Content Protection Key" connector property of each HDCP capable display.
class DISPLAY_MANAGER_EXPORT ContentProtectionKeyManager {
 public:
  using ProvisionedKeyRequest = base::RepeatingCallback<void(
      base::OnceCallback<void(const std::string&)>)>;
  using KeySetCallback = base::OnceCallback<void()>;

  ContentProtectionKeyManager();

  ContentProtectionKeyManager(const ContentProtectionKeyManager&) = delete;
  ContentProtectionKeyManager& operator=(const ContentProtectionKeyManager&) =
      delete;

  ~ContentProtectionKeyManager();

  void set_provisioned_key_request(
      ContentProtectionKeyManager::ProvisionedKeyRequest request) {
    provisioned_key_request_ = std::move(request);
  }

  // Querying the key prop through |native_display_delegate|, requests the key
  // by calling |request| and injects the key into the kernel if required by
  // the |displays_states|.
  // When the displays config is done, whether it was needed or not, on both
  // success and failure, |on_key_set| is called.
  void SetKeyIfRequired(const std::vector<DisplaySnapshot*>& displays_states,
                        NativeDisplayDelegate* native_display_delegate,
                        KeySetCallback on_key_set);

 private:
  enum class KeyFetchingStatus {
    kFetchingNotStarted,
    kFetchingInProgress,
    kFetchingSucceeded,
    kFetchingFailed,
    kFetchingNotRequired
  };

  KeyFetchingStatus key_fetching_status_ =
      KeyFetchingStatus::kFetchingNotStarted;

  // Function to request the provisioned key from the server.
  ProvisionedKeyRequest provisioned_key_request_;
  // It is assumed that the key is the same for all the displays and doesn't
  // change throughout the life of the process.
  std::string cached_provisioned_key_;

  int pending_displays_to_configure_ = 0;
};
}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CONTENT_PROTECTION_KEY_MANAGER_H_
