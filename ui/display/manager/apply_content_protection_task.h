// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_APPLY_CONTENT_PROTECTION_TASK_H_
#define UI_DISPLAY_MANAGER_APPLY_CONTENT_PROTECTION_TASK_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/manager/content_protection_manager.h"
#include "ui/display/types/display_constants.h"

namespace display {

class DisplayLayoutManager;
class NativeDisplayDelegate;

class DISPLAY_MANAGER_EXPORT ApplyContentProtectionTask
    : public ContentProtectionManager::Task {
 public:
  using ResponseCallback = base::OnceCallback<void(Status)>;

  // The task disables protection on displays omitted from |requests|. Note that
  // pending tasks are killed on display reconfiguration.
  ApplyContentProtectionTask(
      DisplayLayoutManager* layout_manager,
      NativeDisplayDelegate* native_display_delegate,
      ContentProtectionManager::ContentProtections requests,
      ResponseCallback callback);

  ApplyContentProtectionTask(const ApplyContentProtectionTask&) = delete;
  ApplyContentProtectionTask& operator=(const ApplyContentProtectionTask&) =
      delete;

  ~ApplyContentProtectionTask() override;

  void Run() override;

 private:
  void OnGetHDCPState(int64_t display_id,
                      bool success,
                      HDCPState state,
                      ContentProtectionMethod protection_method);
  void OnSetHDCPState(bool success);

  uint32_t GetDesiredProtectionMask(int64_t display_id) const;

  const raw_ptr<DisplayLayoutManager> layout_manager_;            // Not owned.
  const raw_ptr<NativeDisplayDelegate> native_display_delegate_;  // Not owned.

  const ContentProtectionManager::ContentProtections requests_;
  ResponseCallback callback_;

  struct HdcpRequest {
    HdcpRequest(int64_t display_id_in,
                HDCPState state_in,
                ContentProtectionMethod protection_method_in)
        : display_id(display_id_in),
          state(state_in),
          protection_method(protection_method_in) {}

    int64_t display_id;
    HDCPState state;
    ContentProtectionMethod protection_method;
  };

  std::vector<HdcpRequest> hdcp_requests_;

  bool success_ = true;
  size_t pending_requests_ = 0;

  base::WeakPtrFactory<ApplyContentProtectionTask> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_APPLY_CONTENT_PROTECTION_TASK_H_
