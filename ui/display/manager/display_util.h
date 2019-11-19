// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_UTIL_H_
#define UI_DISPLAY_MANAGER_DISPLAY_UTIL_H_

#include <string>
#include <vector>

#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"

#if defined(OS_CHROMEOS)
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif  // defined(OS_CHROMEOS)

namespace display {

class DisplaySnapshot;
class ManagedDisplayMode;

#if defined(OS_CHROMEOS)
// Returns a string describing |state|.
std::string DisplayPowerStateToString(chromeos::DisplayPowerState state);

// Returns the number of displays in |displays| that should be turned on, per
// |state|.  If |display_power| is non-NULL, it is updated to contain the
// on/off state of each corresponding entry in |displays|.
int DISPLAY_MANAGER_EXPORT
GetDisplayPower(const std::vector<DisplaySnapshot*>& displays,
                chromeos::DisplayPowerState state,
                std::vector<bool>* display_power);

#endif  // defined(OS_CHROMEOS)

// Returns a string describing |state|.
std::string MultipleDisplayStateToString(MultipleDisplayState state);

// Sets bits in |protection_mask| for each ContentProtectionMethod supported by
// the display |type|. Returns false for unknown display types.
bool GetContentProtectionMethods(DisplayConnectionType type,
                                 uint32_t* protection_mask);

// Returns a list of display zooms supported by the given |mode|.
std::vector<float> DISPLAY_MANAGER_EXPORT
GetDisplayZoomFactors(const ManagedDisplayMode& mode);

// Returns a list of display zooms based on the provided |dsf| of the display.
// This is useful for displays that have a non unity device scale factors
// applied to them.
std::vector<float> DISPLAY_MANAGER_EXPORT GetDisplayZoomFactorForDsf(float dsf);

// This function adds |dsf| to the vector of |zoom_values| by replacing
// the element it is closest to in the list. It also ensures that it never
// replaces the default zoom value of 1.0 from the list and that the size of the
// list never changes.
// TODO(malaykeshav): Remove this after a few milestones.
void DISPLAY_MANAGER_EXPORT InsertDsfIntoList(std::vector<float>* zoom_values,
                                              float dsf);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_UTIL_H_
