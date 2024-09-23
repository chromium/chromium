// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_display_host.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
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

void DrmDisplayHost::SetHdcpKeyProp(const std::string& key,
                                    display::SetHdcpKeyPropCallback callback) {
  set_hdcp_key_prop_callback_ = std::move(callback);
  if (!sender_->GpuSetHdcpKeyProp(snapshot_->display_id(), key)) {
    OnHdcpKeyPropSetReceived(false);
  }
}

void DrmDisplayHost::OnHdcpKeyPropSetReceived(bool success) {
  if (!set_hdcp_key_prop_callback_.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(set_hdcp_key_prop_callback_), success));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  set_hdcp_key_prop_callback_.Reset();
}

void DrmDisplayHost::GetHDCPState(display::GetHDCPStateCallback callback) {
  get_hdcp_callback_ = std::move(callback);
  if (!sender_->GpuGetHDCPState(snapshot_->display_id()))
    OnHDCPStateReceived(false, display::HDCP_STATE_UNDESIRED,
                        display::CONTENT_PROTECTION_METHOD_NONE);
}

void DrmDisplayHost::OnHDCPStateReceived(
    bool status,
    display::HDCPState state,
    display::ContentProtectionMethod protection_method) {
  if (!get_hdcp_callback_.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(get_hdcp_callback_), status, state,
                                  protection_method));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  get_hdcp_callback_.Reset();
}

void DrmDisplayHost::SetHDCPState(
    display::HDCPState state,
    display::ContentProtectionMethod protection_method,
    display::SetHDCPStateCallback callback) {
  set_hdcp_callback_ = std::move(callback);
  if (!sender_->GpuSetHDCPState(snapshot_->display_id(), state,
                                protection_method))
    OnHDCPStateUpdated(false);
}

void DrmDisplayHost::OnHDCPStateUpdated(bool status) {
  if (!set_hdcp_callback_.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(set_hdcp_callback_), status));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  set_hdcp_callback_.Reset();
}

void DrmDisplayHost::SetColorTemperatureAdjustment(
    const display::ColorTemperatureAdjustment& cta) {
  sender_->GpuSetColorTemperatureAdjustment(snapshot_->display_id(), cta);
}

void DrmDisplayHost::SetColorCalibration(
    const display::ColorCalibration& calibration) {
  sender_->GpuSetColorCalibration(snapshot_->display_id(), calibration);
}

void DrmDisplayHost::SetGammaAdjustment(
    const display::GammaAdjustment& adjustment) {
  sender_->GpuSetGammaAdjustment(snapshot_->display_id(), adjustment);
}

void DrmDisplayHost::SetPrivacyScreen(
    bool enabled,
    display::SetPrivacyScreenCallback callback) {
  sender_->GpuSetPrivacyScreen(snapshot_->display_id(), enabled,
                               std::move(callback));
}

void DrmDisplayHost::GetSeamlessRefreshRates(
    display::GetSeamlessRefreshRatesCallback callback) const {
  sender_->GpuGetSeamlessRefreshRates(snapshot_->display_id(),
                                      std::move(callback));
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
  if (!get_hdcp_callback_.is_null())
    OnHDCPStateReceived(false, display::HDCP_STATE_UNDESIRED,
                        display::CONTENT_PROTECTION_METHOD_NONE);
  if (!set_hdcp_callback_.is_null())
    OnHDCPStateUpdated(false);
}

}  // namespace ui
