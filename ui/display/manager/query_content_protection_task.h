// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_QUERY_CONTENT_PROTECTION_TASK_H_
#define UI_DISPLAY_MANAGER_QUERY_CONTENT_PROTECTION_TASK_H_

#include <cstddef>
#include <cstdint>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/manager/content_protection_manager.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"

namespace display {

class DisplayLayoutManager;
class NativeDisplayDelegate;

class DISPLAY_MANAGER_EXPORT QueryContentProtectionTask
    : public ContentProtectionManager::Task {
 public:
  // |connection_mask| includes mirroring displays, and a protection method is
  // only included in |protection_mask| if also enabled on mirroring displays.
  using ResponseCallback = base::OnceCallback<
      void(Status status, uint32_t connection_mask, uint32_t protection_mask)>;

  QueryContentProtectionTask(DisplayLayoutManager* layout_manager,
                             NativeDisplayDelegate* native_display_delegate,
                             int64_t display_id,
                             ResponseCallback callback);

  QueryContentProtectionTask(const QueryContentProtectionTask&) = delete;
  QueryContentProtectionTask& operator=(const QueryContentProtectionTask&) =
      delete;

  ~QueryContentProtectionTask() override;

  void Run() override;

 private:
  void OnGetHDCPState(bool success,
                      HDCPState state,
                      ContentProtectionMethod protection_method);

  const raw_ptr<DisplayLayoutManager> layout_manager_;            // Not owned.
  const raw_ptr<NativeDisplayDelegate> native_display_delegate_;  // Not owned.

  const int64_t display_id_;

  ResponseCallback callback_;

  uint32_t connection_mask_ = DISPLAY_CONNECTION_TYPE_NONE;
  uint32_t protection_mask_ = CONTENT_PROTECTION_METHOD_NONE;
  uint32_t no_protection_mask_ = CONTENT_PROTECTION_METHOD_NONE;

  bool success_ = true;
  size_t pending_requests_ = 0;

  base::WeakPtrFactory<QueryContentProtectionTask> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_QUERY_CONTENT_PROTECTION_TASK_H_
