// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayManagerGpu::DrmOverlayManagerGpu(DrmThreadProxy* drm_thread_proxy)
    : drm_thread_proxy_(drm_thread_proxy) {}

DrmOverlayManagerGpu::~DrmOverlayManagerGpu() = default;

void DrmOverlayManagerGpu::SendOverlayValidationRequest(
    const std::vector<OverlaySurfaceCandidate>& candidates,
    gfx::AcceleratedWidget widget) {
  TRACE_EVENT_ASYNC_BEGIN0(
      "hwoverlays", "DrmOverlayManagerGpu::SendOverlayValidationRequest", this);

  // Adds a callback for the DRM thread to let us know when display
  // configuration has changed and to reset cache of valid overlay
  // configurations. This happens in SendOverlayValidationRequest() because the
  // DrmThread has been started by this point *and* we are on the thread the
  // callback should run on. Those two conditions are not necessarily true in
  // the constructor.
  if (!has_set_clear_cache_callback_) {
    has_set_clear_cache_callback_ = true;
    drm_thread_proxy_->SetClearOverlayCacheCallback(base::BindRepeating(
        &DrmOverlayManagerGpu::ResetCache, weak_ptr_factory_.GetWeakPtr()));
  }

  drm_thread_proxy_->CheckOverlayCapabilities(
      widget, candidates,
      base::BindOnce(&DrmOverlayManagerGpu::ReceiveOverlayValidationResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DrmOverlayManagerGpu::ReceiveOverlayValidationResponse(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates,
    const std::vector<OverlayStatus>& status) {
  TRACE_EVENT_ASYNC_END0(
      "hwoverlays", "DrmOverlayManagerGpu::SendOverlayValidationRequest", this);

  UpdateCacheForOverlayCandidates(candidates, widget, status);
}

}  // namespace ui
