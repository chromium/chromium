// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/vsync_provider_mac.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
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

bool VSyncProviderMac::IsDisplayLinkInBrowserValid(int64_t vsync_display_id) {
  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  if (!task_runner_->BelongsToCurrentThread()) {
    // `display_states_` is updated on the Viz thread. When called on a non-Viz
    // thread (such as `CrGpuMain` or `CompositorGpuThread`), we must acquire
    // `id_lock_` to prevent data races and ensure memory visibility.
    base::AutoLock lock(id_lock_);
    return display_states_.contains(display_id);
  } else {
    return display_states_.contains(display_id);
  }
}

void VSyncProviderMac::SetSupportedDisplayLinkId(int64_t vsync_display_id,
                                                 bool is_supported) {
  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  if (is_supported) {
    AddSupportedDisplayLinkId(display_id);
  } else {
    RemoveSupportedDisplayLinkId(display_id);
  }
}

void VSyncProviderMac::AddSupportedDisplayLinkId(CGDirectDisplayID display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  base::AutoLock lock(id_lock_);
  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    // Insert an empty callback list.
    auto result =
        display_states_.emplace(std::make_pair(display_id, DisplayState()));
    bool inserted = result.second;
    DCHECK(inserted);
  }
}

void VSyncProviderMac::RemoveSupportedDisplayLinkId(
    CGDirectDisplayID display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  base::AutoLock lock(id_lock_);
  display_states_.erase(display_id);
}

void VSyncProviderMac::RegisterCallback(VSyncCallbackMac::Callback callback,
                                        CGDirectDisplayID display_id) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VSyncProviderMac::RegisterCallback,
                                  base::Unretained(this), std::move(callback),
                                  display_id));
    return;
  }

  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    return;
  }
  DisplayState& display_state = found->second;

  std::list<VSyncCallbackMac::Callback>& callbacks = display_state.callbacks;
  bool should_request_begin_frame = callbacks.empty();

  callbacks.push_back(std::move(callback));

  // Request BeginFrames from the browser via IPC.
  if (should_request_begin_frame) {
    display_state.begin_frame_request_time = base::TimeTicks::Now();

    needs_begin_frame_repeating_cb_.Run(display_id,
                                        /*needs_begin_frames=*/true);
  }
}

void VSyncProviderMac::UnregisterCallback(VSyncCallbackMac::Callback callback,
                                          CGDirectDisplayID display_id) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VSyncProviderMac::UnregisterCallback,
                                  base::Unretained(this), std::move(callback),
                                  display_id));
    return;
  }

  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    return;
  }
  DisplayState& display_state = found->second;

  // Reset the BeginFrame request timestamp for this display.
  display_state.begin_frame_request_time = base::TimeTicks();

  std::list<VSyncCallbackMac::Callback>& callbacks = display_state.callbacks;
  callbacks.remove(callback);

  // Stop BeginFrame in browser via IPC.
  if (callbacks.empty()) {
    needs_begin_frame_repeating_cb_.Run(display_id,
                                        /*needs_begin_frames=*/false);
  }
}

void VSyncProviderMac::OnVSync(const VSyncParamsMac& params,
                               int64_t vsync_display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);
  TRACE_EVENT0("gpu", "VSyncProviderMac::OnVSync");

  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  // DisplayLink entry might no longer exist.
  auto found = display_states_.find(display_id);
  if (found == display_states_.end()) {
    return;
  }
  DisplayState& display_state = found->second;

  if (!display_state.begin_frame_request_time.is_null()) {
    RecordTimeFromNeedsBeginFramesToVSync(
        display_state.begin_frame_request_time);
    // Reset the timestamp so that the metric is only recorded once per request.
    display_state.begin_frame_request_time = base::TimeTicks();
  }

  // Unregister() might be called inside the loop and
  // |callback_lists_.[display_id]| size changes while callbacks are called. Get
  // a local copy here.
  std::list<VSyncCallbackMac::Callback> local_callbacks =
      display_state.callbacks;

  // Run callbacks.
  for (auto& cb : local_callbacks) {
    cb.Run(params);
  }
}

void VSyncProviderMac::SetCallbackForRemoteNeedsBeginFrame(
    NeedsBeginFrameCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  needs_begin_frame_repeating_cb_ = std::move(callback);
}

bool VSyncProviderMac::BelongsToCurrentThread() {
  return task_runner_->BelongsToCurrentThread();
}

bool VSyncProviderMac::IsConnectedToBrowserOnVizThread() {
  if (!task_runner_->BelongsToCurrentThread()) {
    return false;
  }

  return !needs_begin_frame_repeating_cb_.is_null();
}

void VSyncProviderMac::RecordTimeFromNeedsBeginFramesToVSync(
    base::TimeTicks begin_frame_request_time) {
  // The kMaxKeepAliveCount is 20 frames in ExternalBeginFrameSourceMac. That
  // means the worse case scenario is logging 6 times per seconds on a 120Hz
  // display. This number is far from the real world cases because most webpages
  // don't do frequent rendering start/stop change.
  base::TimeDelta delta = base::TimeTicks::Now() - begin_frame_request_time;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Viz.DisplayLink.IpcTimeFromNeedsBeginFramesToVSyncReceived", delta,
      base::Milliseconds(1), base::Minutes(30), 50);
}

void VSyncProviderMac::OnSuspend() {
  // Clear `begin_frame_request_time` so the time it takes after power suspend
  // will not be recorded.
  for (auto& display_state : display_states_) {
    display_state.second.begin_frame_request_time = base::TimeTicks();
  }
}

}  // namespace ui
