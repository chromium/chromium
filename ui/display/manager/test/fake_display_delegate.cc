// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/fake_display_delegate.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/display/util/display_util.h"

namespace display {

namespace {

// The EDID specification marks the top bit of the manufacturer id as reserved.
constexpr uint16_t kReservedManufacturerID = 1 << 15;

// A random product name hash.
constexpr uint32_t kProductCodeHash = 3692486807;

// Delay for Configure().
constexpr base::TimeDelta kConfigureDisplayDelay = base::Milliseconds(200);

bool AreModesEqual(const display::DisplayMode& lhs,
                   const display::DisplayMode& rhs) {
  return lhs.size() == rhs.size() &&
         lhs.is_interlaced() == rhs.is_interlaced() &&
         lhs.refresh_rate() == rhs.refresh_rate();
}

}  // namespace

FakeDisplayDelegate::FakeDisplayDelegate() = default;

FakeDisplayDelegate::~FakeDisplayDelegate() = default;

int64_t FakeDisplayDelegate::AddDisplay(const gfx::Size& display_size) {
  DCHECK(!display_size.IsEmpty());

  if (next_display_id_ == 0xFF) {
    LOG(ERROR) << "Exceeded display id limit";
    return kInvalidDisplayId;
  }

  int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                 ++next_display_id_);

  FakeDisplaySnapshot::Builder builder;
  builder.SetId(id).SetNativeMode(display_size);

  return AddDisplay(builder.Build()) ? id : kInvalidDisplayId;
}

bool FakeDisplayDelegate::AddDisplay(std::unique_ptr<DisplaySnapshot> display) {
  DCHECK(display);

  int64_t display_id = display->display_id();
  // Check there is no existing display with the same id.
  for (auto& existing_display : displays_) {
    if (existing_display->display_id() == display_id) {
      LOG(ERROR) << "Display with id " << display_id << " already exists";
      return false;
    }
  }

  DVLOG(1) << "Added display " << display->ToString();
  displays_.push_back(std::move(display));
  OnConfigurationChanged();

  return true;
}

bool FakeDisplayDelegate::RemoveDisplay(int64_t display_id) {
  // Find display snapshot with matching id and remove it.
  for (auto iter = displays_.begin(); iter != displays_.end(); ++iter) {
    if ((*iter)->display_id() == display_id) {
      DVLOG(1) << "Removed display " << (*iter)->ToString();
      displays_.erase(iter);
      OnConfigurationChanged();
      return true;
    }
  }

  return false;
}

void FakeDisplayDelegate::Initialize() {
  DCHECK(!initialized_);

  // The default display will be an internal display with a native resolution
  // of 1366x768 if --screen-config not specified on the command line.
  std::string command_str = "1366x768/i";

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kScreenConfig))
    command_str = command_line->GetSwitchValueASCII(switches::kScreenConfig);

  CreateDisplaysFromSpecString(command_str);
  initialized_ = true;
}

void FakeDisplayDelegate::TakeDisplayControl(DisplayControlCallback callback) {
  std::move(callback).Run(false);
}

void FakeDisplayDelegate::RelinquishDisplayControl(
    DisplayControlCallback callback) {
  std::move(callback).Run(false);
}

void FakeDisplayDelegate::GetDisplays(GetDisplaysCallback callback) {
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> displays;
  for (auto& display : displays_)
    displays.push_back(display.get());
  std::move(callback).Run(displays);
}

void FakeDisplayDelegate::Configure(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    ConfigureCallback callback,
    display::ModesetFlags modeset_flags) {
  bool config_success = true;
  for (const auto& config : config_requests) {
    bool request_success = false;

    if (config.mode) {
      // Find display snapshot of display ID.
      auto snapshot =
          find_if(displays_.begin(), displays_.end(),
                  [&config](std::unique_ptr<DisplaySnapshot>& snapshot) {
                    return snapshot->display_id() == config.id;
                  });
      if (snapshot != displays_.end()) {
        // Check that config mode is appropriate for the display snapshot.
        for (const auto& existing_mode : snapshot->get()->modes()) {
          if (AreModesEqual(*existing_mode.get(), *config.mode)) {
            request_success = true;
            break;
          }
        }
      }
    } else {
      // This is a request to turn off the display.
      request_success = true;
    }
    config_success &= request_success;
  }

  configure_callbacks_.push(
      base::BindOnce(std::move(callback), config_requests, config_success));

  // Start the timer if it's not already running. If there are multiple queued
  // configuration requests then ConfigureDone() will handle starting the
  // next request.
  if (!configure_timer_.IsRunning()) {
    configure_timer_.Start(FROM_HERE, kConfigureDisplayDelay, this,
                           &FakeDisplayDelegate::ConfigureDone);
  }
}

void FakeDisplayDelegate::SetHdcpKeyProp(int64_t display_id,
                                         const std::string& key,
                                         SetHdcpKeyPropCallback callback) {
  std::move(callback).Run(false);
}

void FakeDisplayDelegate::GetHDCPState(const DisplaySnapshot& output,
                                       GetHDCPStateCallback callback) {
  std::move(callback).Run(false, HDCP_STATE_UNDESIRED,
                          CONTENT_PROTECTION_METHOD_NONE);
}

void FakeDisplayDelegate::SetHDCPState(
    const DisplaySnapshot& output,
    HDCPState state,
    ContentProtectionMethod protection_method,
    SetHDCPStateCallback callback) {
  std::move(callback).Run(false);
}

void FakeDisplayDelegate::SetColorTemperatureAdjustment(
    int64_t display_id,
    const ColorTemperatureAdjustment& cta) {}

void FakeDisplayDelegate::SetColorCalibration(
    int64_t display_id,
    const ColorCalibration& calibration) {}

void FakeDisplayDelegate::SetGammaAdjustment(int64_t display_id,
                                             const GammaAdjustment& gamma) {}

void FakeDisplayDelegate::SetPrivacyScreen(int64_t display_id,
                                           bool enabled,
                                           SetPrivacyScreenCallback callback) {
  std::move(callback).Run(false);
}

void FakeDisplayDelegate::GetSeamlessRefreshRates(
    int64_t display_id,
    GetSeamlessRefreshRatesCallback callback) const {
  std::move(callback).Run(std::nullopt);
}

void FakeDisplayDelegate::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeDisplayDelegate::RemoveObserver(NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

FakeDisplayController* FakeDisplayDelegate::GetFakeDisplayController() {
  return this;
}

void FakeDisplayDelegate::CreateDisplaysFromSpecString(const std::string& str) {
  // Start without any displays.
  if (str == "none")
    return;

  // Split on commas and parse each display string.
  for (const std::string& part : base::SplitString(
           str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                   next_display_id_);
    std::unique_ptr<DisplaySnapshot> snapshot =
        FakeDisplaySnapshot::CreateFromSpec(id, part);
    if (snapshot) {
      AddDisplay(std::move(snapshot));
      next_display_id_++;
    } else {
      LOG(FATAL) << "Bad --" << switches::kScreenConfig << " flag provided.";
    }
  }
}

void FakeDisplayDelegate::OnConfigurationChanged() {
  if (!initialized_)
    return;

  observers_.Notify(&NativeDisplayObserver::OnConfigurationChanged);
}

void FakeDisplayDelegate::ConfigureDone() {
  DCHECK(!configure_callbacks_.empty());
  std::move(configure_callbacks_.front()).Run();
  configure_callbacks_.pop();

  // If there are more configuration requests waiting then restart the timer.
  if (!configure_callbacks_.empty()) {
    configure_timer_.Start(FROM_HERE, kConfigureDisplayDelay, this,
                           &FakeDisplayDelegate::ConfigureDone);
  }
}

}  // namespace display
