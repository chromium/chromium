// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/vsync_provider_mac.h"

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"

namespace ui {

// static
VSyncProviderMac* VSyncProviderMac::GetInstance() {
  static base::NoDestructor<VSyncProviderMac> provider;
  return provider.get();
}

VSyncProviderMac::VSyncProviderMac()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

VSyncProviderMac::~VSyncProviderMac() = default;

VSyncProviderMac::DisplayState::DisplayState() = default;
VSyncProviderMac::DisplayState::~DisplayState() = default;
VSyncProviderMac::DisplayState::DisplayState(DisplayState&& other) = default;
VSyncProviderMac::DisplayState& VSyncProviderMac::DisplayState::operator=(
    DisplayState&& other) = default;

void VSyncProviderMac::SetSupportedDisplayLinkId(int64_t display_id,
                                                 bool is_supported) {
  if (is_supported) {
    AddSupportedDisplayLinkId(display_id);
  } else {
    RemoveSupportedDisplayLinkId(display_id);
  }
}

void VSyncProviderMac::AddSupportedDisplayLinkId(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  base::AutoLock lock(id_lock_);
  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    display_states_.emplace(display_id, DisplayState());
  }
}

void VSyncProviderMac::RemoveSupportedDisplayLinkId(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  base::AutoLock lock(id_lock_);
  display_states_.erase(display_id);
}

void VSyncProviderMac::RegisterCallback(VSyncCallbackMac::Callback callback,
                                        int64_t display_id) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&VSyncProviderMac::RegisterCallback,
                                          weak_factory_.GetWeakPtr(),
                                          std::move(callback), display_id));
    return;
  }

  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    return;
  }

  DisplayState& display_state = found->second;
  bool should_request_begin_frame = display_state.callbacks.empty();

  display_state.callbacks.push_back(std::move(callback));

  // Request BeginFrame in browser via IPC.
  if (should_request_begin_frame) {
    needs_begin_frame_callback_.Run(display_id,
                                    /*needs_begin_frames=*/true);
  }
}

void VSyncProviderMac::UnregisterCallback(VSyncCallbackMac::Callback callback,
                                          int64_t display_id) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&VSyncProviderMac::UnregisterCallback,
                                          weak_factory_.GetWeakPtr(),
                                          std::move(callback), display_id));
    return;
  }

  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    return;
  }

  found->second.callbacks.remove(callback);

  // Do not request stopping BeginFrame in browser via IPC at this stage. VSyncs
  // will be stopped in OnVSync() if needed.
}

void VSyncProviderMac::OnVSync(const VSyncParamsMac& params,
                               int64_t display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);
  TRACE_EVENT0("gpu", "VSyncProviderMac::OnVSync");

  // DisplayLink entry might no longer exist after display removal.
  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    return;
  }

  DisplayState& display_state = found->second;

  // Unregister() might be called inside the loop and
  // |display_state.callbacks| size changes while callbacks are called. Get
  // a local copy here.
  std::list<VSyncCallbackMac::Callback> local_callbacks =
      display_state.callbacks;

  // Run callbacks
  for (auto& cb : local_callbacks) {
    cb.Run(params);
  }

  // Defer stopping VSync after the last client unregisters.
  if (!display_state.callbacks.empty()) {
    display_state.consecutive_vsyncs_with_no_callbacks = 0;
    return;
  }

  // Keep VSync alive for a short period.
  display_state.consecutive_vsyncs_with_no_callbacks++;
  if (display_state.consecutive_vsyncs_with_no_callbacks <
      kMaxExtraVSyncCallbacks) {
    return;
  }

  // Now stop BeginFrame in the Browser via IPC.
  display_state.consecutive_vsyncs_with_no_callbacks = 0;
  needs_begin_frame_callback_.Run(display_id,
                                  /*needs_begin_frames=*/false);
}

void VSyncProviderMac::SetCallbackForRemoteNeedsBeginFrame(
    NeedsBeginFrameCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);
  needs_begin_frame_callback_ = std::move(callback);
}

bool VSyncProviderMac::IsDisplayLinkSupported(int64_t display_id) {
  if (!needs_begin_frame_callback_) {
    return false;
  }

  // |display_states_| is updated on Viz thread. A lock is needed when this
  // function is called on CrGpuMain or CompositorGpuThread (DrDC).
  base::AutoLock lock(id_lock_);
  return display_states_.find(display_id) != display_states_.end();
}

bool VSyncProviderMac::BelongsToCurrentThread() {
  return task_runner_->BelongsToCurrentThread();
}

}  // namespace ui
