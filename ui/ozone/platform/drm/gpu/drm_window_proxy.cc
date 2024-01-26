// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

DrmWindowProxy::DrmWindowProxy(gfx::AcceleratedWidget widget,
                               DrmThread* drm_thread)
    : widget_(widget), drm_thread_(drm_thread) {}

DrmWindowProxy::~DrmWindowProxy() = default;

void DrmWindowProxy::SchedulePageFlip(
    std::vector<DrmOverlayPlane> planes,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  base::OnceClosure task = base::BindOnce(
      &DrmThread::SchedulePageFlip, base::Unretained(drm_thread_), widget_,
      std::move(planes), CreateSafeOnceCallback(std::move(submission_callback)),
      CreateSafeOnceCallback(std::move(presentation_callback)));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                     base::Unretained(drm_thread_), std::move(task), nullptr));
}

bool DrmWindowProxy::SupportsGpuFences() const {
  bool is_atomic = false;
  base::OnceClosure task =
      base::BindOnce(&DrmThread::IsDeviceAtomic, base::Unretained(drm_thread_),
                     widget_, &is_atomic);
  PostSyncTask(drm_thread_->task_runner(),
               base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                              base::Unretained(drm_thread_), std::move(task)));
  return is_atomic && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                          switches::kDisableExplicitDmaFences);
}

}  // namespace ui
