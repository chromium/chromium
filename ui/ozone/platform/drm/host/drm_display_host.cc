// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_display_host.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"

namespace ui {

DrmDisplayHost::DrmDisplayHost(GpuThreadAdapter* sender,
                               std::unique_ptr<display::DisplaySnapshot> params,
                               bool is_dummy)
    : sender_(sender), snapshot_(std::move(params)), is_dummy_(is_dummy) {
  sender_->AddGpuThreadObserver(this);
}

DrmDisplayHost::~DrmDisplayHost() {
  sender_->RemoveGpuThreadObserver(this);
  ClearCallbacks();
}

void DrmDisplayHost::UpdateDisplaySnapshot(
    std::unique_ptr<display::DisplaySnapshot> params) {
  snapshot_ = std::move(params);
}

void DrmDisplayHost::Configure(const display::DisplayMode* mode,
                               const gfx::Point& origin,
                               display::ConfigureCallback callback) {
  if (is_dummy_) {
    std::move(callback).Run(true);
    return;
  }

  configure_callback_ = std::move(callback);
  bool status = false;
  if (mode) {
    status = sender_->GpuConfigureNativeDisplay(
        snapshot_->display_id(), GetDisplayModeParams(*mode), origin);
  } else {
    status = sender_->GpuDisableNativeDisplay(snapshot_->display_id());
  }

  if (!status)
    OnDisplayConfigured(false);
}

void DrmDisplayHost::OnDisplayConfigured(bool status) {
  if (!configure_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(configure_callback_), status));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  configure_callback_.Reset();
}

void DrmDisplayHost::GetHDCPState(display::GetHDCPStateCallback callback) {
  get_hdcp_callback_ = std::move(callback);
  if (!sender_->GpuGetHDCPState(snapshot_->display_id()))
    OnHDCPStateReceived(false, display::HDCP_STATE_UNDESIRED);
}

void DrmDisplayHost::OnHDCPStateReceived(bool status,
                                         display::HDCPState state) {
  if (!get_hdcp_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(get_hdcp_callback_), status, state));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  get_hdcp_callback_.Reset();
}

void DrmDisplayHost::SetHDCPState(display::HDCPState state,
                                  display::SetHDCPStateCallback callback) {
  set_hdcp_callback_ = std::move(callback);
  if (!sender_->GpuSetHDCPState(snapshot_->display_id(), state))
    OnHDCPStateUpdated(false);
}

void DrmDisplayHost::OnHDCPStateUpdated(bool status) {
  if (!set_hdcp_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(set_hdcp_callback_), status));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  set_hdcp_callback_.Reset();
}

void DrmDisplayHost::SetColorMatrix(const std::vector<float>& color_matrix) {
  sender_->GpuSetColorMatrix(snapshot_->display_id(), color_matrix);
}

void DrmDisplayHost::SetGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  sender_->GpuSetGammaCorrection(snapshot_->display_id(), degamma_lut,
                                 gamma_lut);
}

void DrmDisplayHost::OnGpuProcessLaunched() {}

void DrmDisplayHost::OnGpuThreadReady() {
  is_dummy_ = false;

  // Note: These responses are done here since the OnChannelDestroyed() is
  // called after OnChannelEstablished().
  ClearCallbacks();
}

void DrmDisplayHost::OnGpuThreadRetired() {}

void DrmDisplayHost::ClearCallbacks() {
  if (!configure_callback_.is_null())
    OnDisplayConfigured(false);
  if (!get_hdcp_callback_.is_null())
    OnHDCPStateReceived(false, display::HDCP_STATE_UNDESIRED);
  if (!set_hdcp_callback_.is_null())
    OnHDCPStateUpdated(false);
}

}  // namespace ui
