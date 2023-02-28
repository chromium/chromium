// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_

#include <vector>

#include "base/functional/identity.h"
#include "base/ranges/algorithm.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/managed_display_info.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace display {

class ManagedDisplayInfo;
class ManagedDisplayMode;
using DisplayInfoList = std::vector<ManagedDisplayInfo>;

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
template <typename Range, typename UnaryOperation = base::identity>
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

// Creates managed display info.
DISPLAY_MANAGER_EXPORT display::ManagedDisplayInfo CreateDisplayInfo(
    int64_t id,
    const gfx::Rect& bounds);

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

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_
