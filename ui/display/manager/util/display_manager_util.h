// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_UTIL_DISPLAY_MANAGER_UTIL_H_
#define UI_DISPLAY_MANAGER_UTIL_DISPLAY_MANAGER_UTIL_H_

#include <functional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace display {

class DisplaySnapshot;
class ManagedDisplayMode;
class ManagedDisplayInfo;
using DisplayInfoList = std::vector<ManagedDisplayInfo>;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns a string describing |state|.
std::string DisplayPowerStateToString(chromeos::DisplayPowerState state);

// Returns a string describing |state|.
std::string VrrStateToString(const base::flat_set<int64_t>& state);

std::string RefreshRateOverrideToString(
    const std::unordered_map<int64_t, float>& refresh_rate_override);

// Returns the number of displays in |displays| that should be turned on, per
// |state|.  If |display_power| is non-NULL, it is updated to contain the
// on/off state of each corresponding entry in |displays|.
DISPLAY_MANAGER_EXPORT int GetDisplayPower(
    const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
    chromeos::DisplayPowerState state,
    std::vector<bool>* display_power);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Determines whether |a| is within an epsilon of |b|.
bool WithinEpsilon(float a, float b);

// Returns a string describing |state|.
std::string MultipleDisplayStateToString(MultipleDisplayState state);

// Sets bits in |protection_mask| for each ContentProtectionMethod supported by
// the display |type|. Returns false for unknown display types.
bool GetContentProtectionMethods(DisplayConnectionType type,
                                 uint32_t* protection_mask);

// Returns a list of display zooms supported by the given |mode|.
DISPLAY_MANAGER_EXPORT std::vector<float> GetDisplayZoomFactors(
    const ManagedDisplayMode& mode);

// Returns a list of display zooms supported by the given |display_width|.
DISPLAY_MANAGER_EXPORT std::vector<float> GetDisplayZoomFactorsByDisplayWidth(
    const int display_width);

// Returns a list of display zooms based on the provided |dsf| of the display.
// This is useful for displays that have a non unity device scale factors
// applied to them.
DISPLAY_MANAGER_EXPORT std::vector<float> GetDisplayZoomFactorForDsf(float dsf);

// Creates the display mode list for internal display
// based on |native_mode|.
DISPLAY_MANAGER_EXPORT ManagedDisplayInfo::ManagedDisplayModeList
CreateInternalManagedDisplayModeList(const ManagedDisplayMode& native_mode);

// Defines parameters needed to construct a ManagedDisplayMode for Unified
// Desktop.
struct UnifiedDisplayModeParam {
  UnifiedDisplayModeParam(float dsf, float scale, bool is_default);

  float device_scale_factor = 1.0f;

  float display_bounds_scale = 1.0f;

  bool is_default_mode = false;
};

// Creates the display mode list for unified display
// based on |native_mode| and |scales|.
DISPLAY_MANAGER_EXPORT ManagedDisplayInfo::ManagedDisplayModeList
CreateUnifiedManagedDisplayModeList(
    const ManagedDisplayMode& native_mode,
    const std::vector<UnifiedDisplayModeParam>& modes_param_list);

// Returns true if the first display should unconditionally be considered an
// internal display.
bool ForceFirstDisplayInternal();

// If |a_bounds| and |b_bounds| share an edge, the shared edges are computed and
// filled in |a_edge| and |b_edge|, and true is returned. Otherwise, it returns
// false.
DISPLAY_MANAGER_EXPORT bool ComputeBoundary(const gfx::Rect& a_bounds,
                                            const gfx::Rect& b_bounds,
                                            gfx::Rect* a_edge,
                                            gfx::Rect* b_edge);

// If |display_a| and |display_b| share an edge, the shared edges are computed
// and filled in |a_edge_in_screen| and |b_edge_in_screen|, and true is
// returned. Otherwise, it returns false.
DISPLAY_MANAGER_EXPORT bool ComputeBoundary(const Display& display_a,
                                            const Display& display_b,
                                            gfx::Rect* a_edge_in_screen,
                                            gfx::Rect* b_edge_in_screen);

// Sorts id list using `CompareDisplayIds()` in display.h.
DISPLAY_MANAGER_EXPORT void SortDisplayIdList(DisplayIdList* list);

// Check if the list is sorted using `CompareDisplayIds()` in display.h.
DISPLAY_MANAGER_EXPORT bool IsDisplayIdListSorted(const DisplayIdList& list);

// Generate sorted DisplayIdList from iterators.
template <typename Range, typename UnaryOperation = std::identity>
DisplayIdList GenerateDisplayIdList(Range&& range, UnaryOperation op = {}) {
  DisplayIdList list;
  base::ranges::transform(range, std::back_inserter(list), op);
  SortDisplayIdList(&list);
  return list;
}

// Creates sorted DisplayIdList.
DISPLAY_MANAGER_EXPORT DisplayIdList CreateDisplayIdList(const Displays& list);
DISPLAY_MANAGER_EXPORT DisplayIdList
CreateDisplayIdList(const DisplayInfoList& updated_displays);

DISPLAY_MANAGER_EXPORT std::string DisplayIdListToString(
    const DisplayIdList& list);

// Get the display id after the output index (8 bits) is masked out.
DISPLAY_MANAGER_EXPORT int64_t GetDisplayIdWithoutOutputIndex(int64_t id);

// Defines parameters needed to set mixed mirror mode.
struct DISPLAY_MANAGER_EXPORT MixedMirrorModeParams {
  MixedMirrorModeParams(int64_t src_id, const DisplayIdList& dst_ids);
  MixedMirrorModeParams(const MixedMirrorModeParams& mixed_params);
  ~MixedMirrorModeParams();

  int64_t source_id;  // Id of the mirroring source display

  DisplayIdList destination_ids;  // Ids of the mirroring destination displays.
};

// Defines mirror modes used to change the display mode.
enum class MirrorMode {
  kOff = 0,
  // Normal mode, with one display mirrored to all other connected displays.
  kNormal,
  // Mixed mode, with one display mirrored to one or more other displays, and
  // the rest of the displays are in EXTENDED mode.
  kMixed,
};

// Defines the error types of mixed mirror mode parameters.
enum class MixedMirrorModeParamsErrors {
  kSuccess = 0,
  kErrorSingleDisplay,
  kErrorSourceIdNotFound,
  kErrorDestinationIdsEmpty,
  kErrorDestinationIdNotFound,
  kErrorDuplicateId,
};

// Verifies whether the mixed mirror mode parameters are valid.
// |connected_display_ids| is the id list for all connected displays. Returns
// error type for the parameters.
DISPLAY_MANAGER_EXPORT MixedMirrorModeParamsErrors
ValidateParamsForMixedMirrorMode(
    const DisplayIdList& connected_display_ids,
    const MixedMirrorModeParams& mixed_mode_params);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_UTIL_DISPLAY_MANAGER_UTIL_H_
