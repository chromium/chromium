// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_UTIL_H_
#define UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"

namespace display {

struct ColorCalibration;
struct ColorTemperatureAdjustment;
struct DisplayConfigurationParams;
struct GammaAdjustment;

namespace test {

// Strings returned by TestNativeDisplayDelegate::GetActionsAndClear() to
// describe various actions that were performed.
const char kInit[] = "init";
const char kTakeDisplayControl[] = "take";
const char kRelinquishDisplayControl[] = "relinquish";

// String returned by TestNativeDisplayDelegate::GetActionsAndClear() if no
// actions were requested.
const char kNoActions[] = "";

// Returns a string describing a TestNativeDisplayDelegate::Configure()
// call.
std::string GetCrtcAction(
    const display::DisplayConfigurationParams& display_config_params);

// Returns a string describing a TestNativeDisplayDelegate::SetHDCPState() call.
std::string GetSetHDCPStateAction(int64_t display_id,
                                  HDCPState state,
                                  ContentProtectionMethod protection_method);

// Returns a string describing a TestNativeDisplayDelegate::SetHdcpKeyProp()
// call.
std::string GetSetHdcpKeyPropAction(int64_t display_id, bool success);

// Returns a string describing a
// TestNativeDisplayDelegate::SetColorCalibration() call.
std::string SetColorCalibrationAction(
    int64_t display_id,
    const display::ColorCalibration& calibration);

// Returns a string describing a
// TestNativeDisplayDelegate::SetColorTemperatureAdjustment() call.
std::string SetColorTemperatureAdjustmentAction(
    int64_t display_id,
    const display::ColorTemperatureAdjustment& cta);

// Returns a string describing a TestNativeDisplayDelegate::SetGammaAdjustment()
// call.
std::string SetGammaAdjustmentAction(int64_t display_id,
                                     const display::GammaAdjustment& gamma);

// Returns a string describing a TestNativeDisplayDelegate::SetPrivacyScreen()
// call.
std::string SetPrivacyScreenAction(int64_t display_id, bool enabled);

// Joins a sequence of strings describing actions (e.g. kScreenDim) such
// that they can be compared against a string returned by
// ActionLogger::GetActionsAndClear().  The list of actions must be
// terminated by a NULL pointer.
std::string JoinActions(const char* action, ...);

}  // namespace test

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_UTIL_H_
