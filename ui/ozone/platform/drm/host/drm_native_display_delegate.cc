// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"

#include <utility>

#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/ozone/platform/drm/host/drm_display_host.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"

namespace ui {

DrmNativeDisplayDelegate::DrmNativeDisplayDelegate(
    DrmDisplayHostManager* display_manager)
    : display_manager_(display_manager) {}

DrmNativeDisplayDelegate::~DrmNativeDisplayDelegate() {
  display_manager_->RemoveDelegate(this);
}

void DrmNativeDisplayDelegate::OnConfigurationChanged() {
  observers_.Notify(&display::NativeDisplayObserver::OnConfigurationChanged);
}

void DrmNativeDisplayDelegate::OnDisplaySnapshotsInvalidated() {
  observers_.Notify(
      &display::NativeDisplayObserver::OnDisplaySnapshotsInvalidated);
}

void DrmNativeDisplayDelegate::Initialize() {
  display_manager_->AddDelegate(this);
}

void DrmNativeDisplayDelegate::TakeDisplayControl(
    display::DisplayControlCallback callback) {
  display_manager_->TakeDisplayControl(std::move(callback));
}

void DrmNativeDisplayDelegate::RelinquishDisplayControl(
    display::DisplayControlCallback callback) {
  display_manager_->RelinquishDisplayControl(std::move(callback));
}

void DrmNativeDisplayDelegate::GetDisplays(
    display::GetDisplaysCallback callback) {
  display_manager_->UpdateDisplays(std::move(callback));
}

void DrmNativeDisplayDelegate::Configure(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    display::ConfigureCallback callback,
    display::ModesetFlags modeset_flags) {
  display_manager_->ConfigureDisplays(config_requests, std::move(callback),
                                      modeset_flags);
}

void DrmNativeDisplayDelegate::SetHdcpKeyProp(
    int64_t display_id,
    const std::string& key,
    display::SetHdcpKeyPropCallback callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
  display->SetHdcpKeyProp(key, std::move(callback));
}

void DrmNativeDisplayDelegate::GetHDCPState(
    const display::DisplaySnapshot& output,
    display::GetHDCPStateCallback callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(output.display_id());
  display->GetHDCPState(std::move(callback));
}

void DrmNativeDisplayDelegate::SetHDCPState(
    const display::DisplaySnapshot& output,
    display::HDCPState state,
    display::ContentProtectionMethod protection_method,
    display::SetHDCPStateCallback callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(output.display_id());
  display->SetHDCPState(state, protection_method, std::move(callback));
}

void DrmNativeDisplayDelegate::SetColorTemperatureAdjustment(
    int64_t display_id,
    const display::ColorTemperatureAdjustment& cta) {
  DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
  display->SetColorTemperatureAdjustment(cta);
}

void DrmNativeDisplayDelegate::SetColorCalibration(
    int64_t display_id,
    const display::ColorCalibration& calibration) {
  DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
  display->SetColorCalibration(calibration);
}

void DrmNativeDisplayDelegate::SetGammaAdjustment(
    int64_t display_id,
    const display::GammaAdjustment& adjustment) {
  DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
  display->SetGammaAdjustment(adjustment);
}

void DrmNativeDisplayDelegate::SetPrivacyScreen(
    int64_t display_id,
    bool enabled,
    display::SetPrivacyScreenCallback callback) {
  DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
  display->SetPrivacyScreen(enabled, std::move(callback));
}

void DrmNativeDisplayDelegate::GetSeamlessRefreshRates(
    int64_t display_id,
    display::GetSeamlessRefreshRatesCallback callback) const {
  DrmDisplayHost* display = display_manager_->GetDisplay(display_id);
  display->GetSeamlessRefreshRates(std::move(callback));
}

void DrmNativeDisplayDelegate::AddObserver(
    display::NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DrmNativeDisplayDelegate::RemoveObserver(
    display::NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::FakeDisplayController*
DrmNativeDisplayDelegate::GetFakeDisplayController() {
  return nullptr;
}

}  // namespace ui
