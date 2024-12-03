// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_api.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "wolvic/jni_headers/VRManager_jni.h"

namespace wolvic {

WvrApi::WvrApi() {
  JNIEnv* env = base::android::AttachCurrentThread();
  shmem_ = reinterpret_cast<mozilla::gfx::VRExternalShmem*>(
      content::Java_VRManager_getExternalContext(env));
  memset((void*)&browser_state_, 0, sizeof(mozilla::gfx::VRBrowserState));
  memset((void*)&system_state_, 0, sizeof(mozilla::gfx::VRSystemState));
}

void WvrApi::PushState(NotifyCondition notify_cond) {
  DCHECK(shmem_);

  if (pthread_mutex_lock((pthread_mutex_t*)&(shmem_->geckoMutex)) == 0) {
    memcpy((void*)&(shmem_->geckoState), (void*)&browser_state_,
           sizeof(mozilla::gfx::VRBrowserState));
    if (notify_cond == NotifyCondition::YES) {
      pthread_cond_signal((pthread_cond_t*)&(shmem_->geckoCond));
    }
    pthread_mutex_unlock((pthread_mutex_t*)&(shmem_->geckoMutex));
  }
}

void WvrApi::PullState(const std::function<bool()>& wait_condition) {
  DCHECK(shmem_);

  bool done = false;
  while (!done) {
    if (pthread_mutex_lock((pthread_mutex_t*)&shmem_->systemMutex) == 0) {
      while (true) {
        memcpy(&system_state_, (void*)&shmem_->state,
               sizeof(mozilla::gfx::VRSystemState));
        if (!wait_condition || wait_condition()) {
          done = true;
          break;
        }
        // Block current thead using the condition variable until data
        // changes
        pthread_cond_wait((pthread_cond_t*)&shmem_->systemCond,
                          (pthread_mutex_t*)&shmem_->systemMutex);
      }
      pthread_mutex_unlock((pthread_mutex_t*)&(shmem_->systemMutex));
    } else if (!wait_condition) {
      // pthread_mutex_lock failed and we are not waiting for a condition to
      // exit from PullState call.
      return;
    }
  }
}

void WvrApi::StartWebXR() {
  // Indicate that we are ready to start immersive mode
  browser_state_.presentationActive = true;
  browser_state_.layerState[0].type =
      mozilla::gfx::VRLayerType::LayerType_Stereo_Immersive;
  PushState();

  PullState([&]() {
    return !system_state_.displayState.suppressFrames &&
           system_state_.displayState.isConnected;
  });

  presenting_generation_ = system_state_.displayState.presentingGeneration;
}

void WvrApi::ExitWebXR() {
  browser_state_.presentationActive = false;
  browser_state_.layerState[0].type = mozilla::gfx::VRLayerType::LayerType_None;
  PushState(NotifyCondition::YES);
}

bool WvrApi::PresentingGenerationChanged() {
  return presenting_generation_ !=
         system_state_.displayState.presentingGeneration;
}

bool WvrApi::SyncState(bool is_frame_submmitted,
                       int32_t texture_handle,
                       const gfx::Size& size,
                       mozilla::gfx::VRDisplayBlendMode blend_mode,
                       mozilla::gfx::ImmersiveXRSessionType session_type) {
  if (is_frame_submmitted) {
    ++sync_frame_index_;
  }

  auto& layer = browser_state_.layerState[0].layer_stereo_immersive;
  layer.frameId = sync_frame_index_;
  layer.textureSize.width = size.width();
  layer.textureSize.height = size.height();
  layer.textureHandle = texture_handle;

  // for (auto& view: views) {
  for (int i = 0; i < 2; ++i) {
    auto& externalRect = i == 0 ? layer.leftEyeRect : layer.rightEyeRect;

    // TODO : How to get this values?
    externalRect.x = i == 0 ? 0 : 0.5f;
    externalRect.y = 0;
    externalRect.width = 0.5f;
    externalRect.height = 1.0f;
  }

  browser_state_.dropFrame = !is_frame_submmitted;
  browser_state_.blendMode = blend_mode;
  browser_state_.sessionType = session_type;
  uint64_t oldDroppedFrameCount = system_state_.displayState.droppedFrameCount;
  PushState(NotifyCondition::YES);
  PullState([this, oldDroppedFrameCount]() {
    return (system_state_.displayState.lastSubmittedFrameId == sync_frame_index_ &&
           (!browser_state_.dropFrame || system_state_.displayState.droppedFrameCount != oldDroppedFrameCount)) ||
           system_state_.displayState.suppressFrames ||
           !system_state_.displayState.isConnected;
  });

  // Avoid racing texture between processing in chromium and consuming in
  // wolvic.
  layer.textureHandle = 0;
  PushState(NotifyCondition::YES);

  return true;
}

void WvrApi::PullSystemState() {
  PullState([&]() {
    return !system_state_.displayState.suppressFrames &&
           system_state_.displayState.isConnected;
  });
}

}  // namespace wolvic
