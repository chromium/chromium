// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_UTIL_H_
#define UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ui/display/types/display_constants.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace display {

struct GammaRampRGBEntry;
class DisplayMode;
class DisplaySnapshot;

namespace test {

// Strings returned by TestNativeDisplayDelegate::GetActionsAndClear() to
// describe various actions that were performed.
const char kInit[] = "init";
const char kTakeDisplayControl[] = "take";
const char kRelinquishDisplayControl[] = "relinquish";

// String returned by TestNativeDisplayDelegate::GetActionsAndClear() if no
// actions were requested.
const char kNoActions[] = "";

std::string DisplaySnapshotToString(const DisplaySnapshot& output);

// Returns a string describing a TestNativeDisplayDelegate::Configure()
// call.
std::string GetCrtcAction(const DisplaySnapshot& output,
                          const DisplayMode* mode,
                          const gfx::Point& origin);

// Returns a string describing a TestNativeDisplayDelegate::SetHDCPState() call.
std::string GetSetHDCPStateAction(int64_t display_id, HDCPState state);

// Returns a string describing a TestNativeDisplayDelegate::SetColorMatrix()
// call.
std::string SetColorMatrixAction(int64_t display_id,
                                 const std::vector<float>& color_matrix);

// Returns a string describing a TestNativeDisplayDelegate::SetGammaCorrection()
// call.
std::string SetGammaCorrectionAction(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut);

// Joins a sequence of strings describing actions (e.g. kScreenDim) such
// that they can be compared against a string returned by
// ActionLogger::GetActionsAndClear().  The list of actions must be
// terminated by a NULL pointer.
std::string JoinActions(const char* action, ...);

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TEST_ACTION_LOGGER_UTIL_H_
