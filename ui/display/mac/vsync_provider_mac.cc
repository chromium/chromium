// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/vsync_provider_mac.h"

#include "base/logging.h"
#include "base/task/bind_post_task.h"

namespace ui {

// static
VSyncProviderMac* VSyncProviderMac::GetInstance() {
  static base::NoDestructor<VSyncProviderMac> provider;
  return provider.get();
}

VSyncProviderMac::VSyncProviderMac()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

VSyncProviderMac::~VSyncProviderMac() = default;

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
  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    std::list<VSyncCallbackMac::Callback> callbacks;
    // Insert an empty callback list
    auto result = callback_lists_.insert(
        std::make_pair(display_id, std::move(callbacks)));
    bool inserted = result.second;
    DCHECK(inserted);
  }
}

void VSyncProviderMac::RemoveSupportedDisplayLinkId(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  base::AutoLock lock(id_lock_);
  auto found = callback_lists_.find(display_id);
  if (found != callback_lists_.end()) {
    callback_lists_.erase(display_id);
  }
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

  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    return;
  }

  std::list<VSyncCallbackMac::Callback>& callbacks = found->second;
  bool should_request_begin_frame = callbacks.empty();

  callbacks.push_back(std::move(callback));

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

  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    return;
  }

  std::list<VSyncCallbackMac::Callback>& callbacks = found->second;
  callbacks.remove(callback);

  // Stop BeginFrame in browser via IPC.
  if (callbacks.empty()) {
    needs_begin_frame_callback_.Run(display_id,
                                    /*needs_begin_frames=*/false);
  }
}

void VSyncProviderMac::OnVSync(const VSyncParamsMac& params,
                               int64_t display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_sequence_checker_);

  // DisplayLink entry might no longer exist.
  auto found = callback_lists_.find(display_id);
  if (found == callback_lists_.end()) {
    return;
  }

  // Unregister() might be called inside the loop and
  // |callback_lists_.[display_id]| size changes while callbacks are called. Get
  // a local copy here.
  std::list<VSyncCallbackMac::Callback> local_callbacks = found->second;

  // Run callbacks
  for (auto& cb : local_callbacks) {
    cb.Run(params);
  }
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

  // |callback_lists_| is updated on Viz thread. A lock is needed when this
  // function is called on CrGpuMain or CompositorGpuThread (DrDC).
  base::AutoLock lock(id_lock_);
  return callback_lists_.find(display_id) != callback_lists_.end();
}

bool VSyncProviderMac::BelongsToCurrentThread() {
  return task_runner_->BelongsToCurrentThread();
}

}  // namespace ui
