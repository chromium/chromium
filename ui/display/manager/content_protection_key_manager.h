// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CONTENT_PROTECTION_KEY_MANAGER_H_
#define UI_DISPLAY_MANAGER_CONTENT_PROTECTION_KEY_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
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
//  2. Look for the "Content Protection Key" connector property to verify if the
//     key is required.
//  3. Check for keys that are cached for the HDCP capable displays, if they
//     don't exist get the provisioned key from the server.
//  4. Inject the key into the kernel by writing the key into the
//     "Content Protection Key" connector property of each HDCP capable display.
class DISPLAY_MANAGER_EXPORT ContentProtectionKeyManager {
 public:
  using ProvisionedKeyRequest = base::RepeatingCallback<void(
      base::OnceCallback<void(const std::string&)>)>;
  using KeySetCallback = base::OnceCallback<void(bool)>;

  ContentProtectionKeyManager();

  ContentProtectionKeyManager(const ContentProtectionKeyManager&) = delete;
  ContentProtectionKeyManager& operator=(const ContentProtectionKeyManager&) =
      delete;

  ~ContentProtectionKeyManager();

  void set_provisioned_key_request(ProvisionedKeyRequest request) {
    provisioned_key_request_ = std::move(request);
  }

  void set_native_display_delegate(NativeDisplayDelegate* delegate) {
    native_display_delegate_ = delegate;
  }

  // Check for the key prop of |displays_states|, request the key by calling
  // |provisioned_key_request_| and inject the key into the kernel if
  // required. When the displays config is done, call |on_key_set|.
  void SetKeyIfRequired(
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>&
          displays_states,
      int64_t display_id,
      KeySetCallback on_key_set);

 private:
  void FetchKeyFromServer();
  void OnKeyFetchedFromServer(const std::string& key);
  void InjectKeyToKernel(int64_t display_id);
  void OnKeyInjectedToKernel(int64_t display_id, bool success);
  void TriggerPendingCallbacks(int64_t callback_id, bool is_key_set);

  // Function to request the provisioned key from the server.
  ProvisionedKeyRequest provisioned_key_request_;
  // It is assumed that the key is the same for all the displays and doesn't
  // change throughout the life of the process.
  std::string cached_provisioned_key_;

  raw_ptr<NativeDisplayDelegate> native_display_delegate_ =
      nullptr;  // Not owned.

  base::flat_map<int64_t, KeySetCallback> pending_display_callbacks_;
  base::flat_set<int64_t> displays_pending_set_key_;

  base::WeakPtrFactory<ContentProtectionKeyManager> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CONTENT_PROTECTION_KEY_MANAGER_H_
