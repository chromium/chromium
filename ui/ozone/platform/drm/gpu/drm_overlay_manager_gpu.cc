// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager_gpu.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayManagerGpu::DrmOverlayManagerGpu(
    DrmThreadProxy* drm_thread_proxy,
    bool handle_overlays_swap_failure,
    bool allow_sync_and_real_buffer_page_flip_testing)
    : DrmOverlayManager(handle_overlays_swap_failure,
                        allow_sync_and_real_buffer_page_flip_testing),
      drm_thread_proxy_(drm_thread_proxy) {}

DrmOverlayManagerGpu::~DrmOverlayManagerGpu() = default;

void DrmOverlayManagerGpu::SendOverlayValidationRequest(
    const std::vector<OverlaySurfaceCandidate>& candidates,
    gfx::AcceleratedWidget widget) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "hwoverlays", "DrmOverlayManagerGpu::SendOverlayValidationRequest",
      TRACE_ID_LOCAL(this));
  SetDisplaysConfiguredCallbackIfNecessary();
  drm_thread_proxy_->CheckOverlayCapabilities(
      widget, candidates,
      base::BindOnce(&DrmOverlayManagerGpu::ReceiveOverlayValidationResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::vector<OverlayStatus>
DrmOverlayManagerGpu::SendOverlayValidationRequestSync(
    const std::vector<OverlaySurfaceCandidate>& candidates,
    gfx::AcceleratedWidget widget) {
  TRACE_EVENT0("hwoverlays",
               "DrmOverlayManagerGpu::SendOverlayValidationRequestSync");
  SetDisplaysConfiguredCallbackIfNecessary();
  return drm_thread_proxy_->CheckOverlayCapabilitiesSync(widget, candidates);
}

void DrmOverlayManagerGpu::GetHardwareCapabilities(
    gfx::AcceleratedWidget widget,
    HardwareCapabilitiesCallback& receive_callback) {
  TRACE_EVENT0("hwoverlays",
               "DrmOverlayManagerGpu::SendMaxOverlaysRequestSync");
  SetDisplaysConfiguredCallbackIfNecessary();
  drm_thread_proxy_->GetHardwareCapabilities(widget, receive_callback);
}

void DrmOverlayManagerGpu::SetDisplaysConfiguredCallbackIfNecessary() {
  // Adds a callback for the DRM thread to let us know when display
  // configuration may have changed.
  // This happens in SendOverlayValidationRequest() because the DrmThread has
  // been started by this point *and* we are on the thread the callback should
  // run on. Those two conditions are not necessarily true in the constructor.
  if (!has_set_displays_configured_callback_) {
    has_set_displays_configured_callback_ = true;
    drm_thread_proxy_->SetDisplaysConfiguredCallback(
        base::BindRepeating(&DrmOverlayManagerGpu::DisplaysConfigured,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void DrmOverlayManagerGpu::ReceiveOverlayValidationResponse(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates,
    const std::vector<OverlayStatus>& status) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "hwoverlays", "DrmOverlayManagerGpu::SendOverlayValidationRequest",
      TRACE_ID_LOCAL(this));

  UpdateCacheForOverlayCandidates(candidates, widget, status);
}

}  // namespace ui
