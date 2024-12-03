// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_API_H_
#define WOLVIC_BROWSER_VR_WVR_API_H_

#include <cstdint>
#include <functional>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "wolvic/browser/vr/moz_external_vr.h"

namespace wolvic {

class WvrApi {
 public:
  WvrApi();

  WvrApi(const WvrApi&) = delete;
  WvrApi& operator=(const WvrApi&) = delete;

  ~WvrApi() = default;

  void StartWebXR();
  void ExitWebXR();
  bool PresentingGenerationChanged();
  bool SyncState(bool is_frame_submmitted,
                 int32_t texture_handle,
                 const gfx::Size& size,
                 mozilla::gfx::VRDisplayBlendMode,
                 mozilla::gfx::ImmersiveXRSessionType);
  void PullSystemState();

  mozilla::gfx::VRSystemState& get_system_state() { return system_state_; }

 private:
  enum class NotifyCondition { YES, NO };
  void PushState(NotifyCondition notifyCond = NotifyCondition::NO);
  void PullState(const std::function<bool()>& waitCondition = {});

  // Communicate via mozilla shared memory.
  mozilla::gfx::VRBrowserState browser_state_;
  mozilla::gfx::VRSystemState system_state_;
  raw_ptr<mozilla::gfx::VRExternalShmem> shmem_;

  uint32_t presenting_generation_;
  uint64_t sync_frame_index_ = 0;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_API_H_
