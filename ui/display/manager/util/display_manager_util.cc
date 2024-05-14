// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/util/display_manager_util.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <set>
#include <sstream>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

namespace display {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "ALL_ON";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "ALL_OFF";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "INTERNAL_OFF_EXTERNAL_ON";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "INTERNAL_ON_EXTERNAL_OFF";
    default:
      return "unknown (" + base::NumberToString(state) + ")";
  }
}

std::string VrrStateToString(const base::flat_set<int64_t>& state) {
  std::vector<std::string> entries;
  for (const int64_t id : state) {
    entries.push_back(base::NumberToString(id));
  }
  return "{" + base::JoinString(entries, ", ") + "}";
}

std::string RefreshRateOverrideToString(
    const std::unordered_map<int64_t, float>& refresh_rate_override) {
  std::vector<std::string> entries;
  for (const auto& [id, refresh_rate] : refresh_rate_override) {
    entries.push_back(base::StringPrintf("%" PRId64 ": %f", id, refresh_rate));
  }
  return "{" + base::JoinString(entries, ", ") + "}";
}

int GetDisplayPower(
    const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
    chromeos::DisplayPowerState state,
    std::vector<bool>* display_power) {
  int num_on_displays = 0;
  if (display_power) {
    display_power->resize(displays.size());
  }

  for (size_t i = 0; i < displays.size(); ++i) {
    bool internal = displays[i]->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool on =
        state == chromeos::DISPLAY_POWER_ALL_ON ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON &&
         !internal) ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (display_power) {
      (*display_power)[i] = on;
    }
    if (on) {
      num_on_displays++;
    }
  }
  return num_on_displays;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

std::string MultipleDisplayStateToString(MultipleDisplayState state) {
  switch (state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      return "INVALID";
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      return "HEADLESS";
    case MULTIPLE_DISPLAY_STATE_SINGLE:
      return "SINGLE";
    case MULTIPLE_DISPLAY_STATE_MULTI_MIRROR:
      return "DUAL_MIRROR";
    case MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED:
      return "MULTI_EXTENDED";
  }
  NOTREACHED_IN_MIGRATION() << "Unknown state " << state;
  return "INVALID";
}

bool GetContentProtectionMethods(DisplayConnectionType type,
                                 uint32_t* protection_mask) {
  switch (type) {
    case DISPLAY_CONNECTION_TYPE_NONE:
    case DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return false;

    case DISPLAY_CONNECTION_TYPE_INTERNAL:
    case DISPLAY_CONNECTION_TYPE_VGA:
    case DISPLAY_CONNECTION_TYPE_NETWORK:
      *protection_mask = CONTENT_PROTECTION_METHOD_NONE;
      return true;

    case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
    case DISPLAY_CONNECTION_TYPE_DVI:
    case DISPLAY_CONNECTION_TYPE_HDMI:
      *protection_mask = CONTENT_PROTECTION_METHOD_HDCP;
      return true;
  }
}

std::vector<float> GetDisplayZoomFactors(const ManagedDisplayMode& mode) {
  // Internal displays have an internal device scale factor greater than 1
  // associated with them. This means that if we use the usual logic, we would
  // end up with a list of zoom levels that the user may not find very useful.
  // Take for example the pixelbook with device scale factor of 2. Based on the
  // usual approach, we would get a zoom range of 90% to 150%. This means:
  //   1. Users will not be able to go to the native resolution which is
  //      achieved at 50% zoom level.
  //   2. Due to the device scale factor, the display already has a low DPI and
  //      users dont want to zoom in, they mostly want to zoom out and add more
  //      pixels to the screen. But we only provide a zoom list of 90% to 150%.
  // This clearly shows we need a different logic to handle internal displays
  // which have lower DPI due to the device scale factor associated with them.
  //
  // OTOH if we look at an external display with a device scale factor of 1 but
  // the same resolution as the pixel book, the DPI would usually be very high
  // and users mostly want to zoom in to reduce the number of pixels on the
  // screen. So having a range of 90% to 130% makes sense.
  // TODO(malaykeshav): Investigate if we can use DPI instead of resolution or
  // device scale factor to decide the list of zoom levels.
  if (mode.device_scale_factor() > 1.f) {
    return GetDisplayZoomFactorForDsf(mode.device_scale_factor());
  }

  // There may be cases where the device scale factor is less than 1. This can
  // happen during testing or local linux builds.
  const int effective_width = std::round(
      static_cast<float>(std::max(mode.size().width(), mode.size().height())) /
      mode.device_scale_factor());
  return GetDisplayZoomFactorsByDisplayWidth(effective_width);
}

std::vector<float> GetDisplayZoomFactorsByDisplayWidth(
    const int display_width) {
  std::size_t index = kZoomListBuckets.size() - 1;
  while (index > 0 && display_width < kZoomListBuckets[index].first) {
    index--;
  }
  DCHECK_GE(display_width, kZoomListBuckets[index].first);

  const auto& zoom_array = kZoomListBuckets[index].second;
  return std::vector<float>(zoom_array.begin(), zoom_array.end());
}

std::vector<float> GetDisplayZoomFactorForDsf(float dsf) {
  DCHECK(!WithinEpsilon(dsf, 1.f));
  DCHECK_GT(dsf, 1.f);

  for (const auto& bucket : kZoomListBucketsForDsf) {
    if (WithinEpsilon(bucket.first, dsf)) {
      return std::vector<float>(bucket.second.begin(), bucket.second.end());
    }
  }
  NOTREACHED_IN_MIGRATION() << "Received a DSF not on the list: " << dsf;
  return {1.f / dsf, 1.f};
}

ManagedDisplayInfo::ManagedDisplayModeList CreateInternalManagedDisplayModeList(
    const ManagedDisplayMode& native_mode) {
  ManagedDisplayMode mode(native_mode.size(), native_mode.refresh_rate(),
                          native_mode.is_interlaced(), true,
                          native_mode.device_scale_factor());
  return ManagedDisplayInfo::ManagedDisplayModeList{mode};
}

UnifiedDisplayModeParam::UnifiedDisplayModeParam(float dsf,
                                                 float scale,
                                                 bool is_default)
    : device_scale_factor(dsf),
      display_bounds_scale(scale),
      is_default_mode(is_default) {}

ManagedDisplayInfo::ManagedDisplayModeList CreateUnifiedManagedDisplayModeList(
    const ManagedDisplayMode& native_mode,
    const std::vector<UnifiedDisplayModeParam>& modes_param_list) {
  ManagedDisplayInfo::ManagedDisplayModeList display_mode_list;
  display_mode_list.reserve(modes_param_list.size());

  for (auto& param : modes_param_list) {
    gfx::SizeF scaled_size(native_mode.size());
    scaled_size.Scale(param.display_bounds_scale);
    display_mode_list.emplace_back(
        gfx::ToFlooredSize(scaled_size), native_mode.refresh_rate(),
        native_mode.is_interlaced(),
        param.is_default_mode ? true : false /* native */,
        param.device_scale_factor);
  }
  // Sort the mode by the size in DIP.
  std::sort(display_mode_list.begin(), display_mode_list.end(),
            [](const ManagedDisplayMode& a, const ManagedDisplayMode& b) {
              return a.GetSizeInDIP().GetArea() < b.GetSizeInDIP().GetArea();
            });
  return display_mode_list;
}

bool ForceFirstDisplayInternal() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool ret = command_line->HasSwitch(::switches::kUseFirstDisplayAsInternal);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Touch view mode is only available to internal display. We force the
  // display as internal for emulator to test touch view mode.
  // However, display mode change is only available to external display. To run
  // tests on a different display mode from default we will need to set the flag
  // --drm-virtual-connector-is-external.
  ret = ret ||
        (ash::system::StatisticsProvider::GetInstance()->IsRunningOnVm() &&
         !command_line->HasSwitch(switches::kDRMVirtualConnectorIsExternal));
#endif
  return ret;
}

bool ComputeBoundary(const gfx::Rect& a_bounds,
                     const gfx::Rect& b_bounds,
                     gfx::Rect* a_edge,
                     gfx::Rect* b_edge) {
  // Find touching side.
  int rx = std::max(a_bounds.x(), b_bounds.x());
  int ry = std::max(a_bounds.y(), b_bounds.y());
  int rr = std::min(a_bounds.right(), b_bounds.right());
  int rb = std::min(a_bounds.bottom(), b_bounds.bottom());

  DisplayPlacement::Position position;
  if (rb == ry) {
    // top bottom
    if (rr <= rx) {
      // Top and bottom align, but no edges are shared.
      return false;
    }

    if (a_bounds.bottom() == b_bounds.y()) {
      position = DisplayPlacement::BOTTOM;
    } else if (a_bounds.y() == b_bounds.bottom()) {
      position = DisplayPlacement::TOP;
    } else {
      return false;
    }
  } else if (rr == rx) {
    // left right
    if (rb <= ry) {
      // Left and right align, but no edges are shared.
      return false;
    }

    if (a_bounds.right() == b_bounds.x()) {
      position = DisplayPlacement::RIGHT;
    } else if (a_bounds.x() == b_bounds.right()) {
      position = DisplayPlacement::LEFT;
    } else {
      return false;
    }
  } else {
    return false;
  }

  switch (position) {
    case DisplayPlacement::TOP:
    case DisplayPlacement::BOTTOM: {
      int left = std::max(a_bounds.x(), b_bounds.x());
      int right = std::min(a_bounds.right(), b_bounds.right());
      if (position == DisplayPlacement::TOP) {
        a_edge->SetRect(left, a_bounds.y(), right - left, 1);
        b_edge->SetRect(left, b_bounds.bottom() - 1, right - left, 1);
      } else {
        a_edge->SetRect(left, a_bounds.bottom() - 1, right - left, 1);
        b_edge->SetRect(left, b_bounds.y(), right - left, 1);
      }
      break;
    }
    case DisplayPlacement::LEFT:
    case DisplayPlacement::RIGHT: {
      int top = std::max(a_bounds.y(), b_bounds.y());
      int bottom = std::min(a_bounds.bottom(), b_bounds.bottom());
      if (position == DisplayPlacement::LEFT) {
        a_edge->SetRect(a_bounds.x(), top, 1, bottom - top);
        b_edge->SetRect(b_bounds.right() - 1, top, 1, bottom - top);
      } else {
        a_edge->SetRect(a_bounds.right() - 1, top, 1, bottom - top);
        b_edge->SetRect(b_bounds.x(), top, 1, bottom - top);
      }
      break;
    }
  }
  return true;
}

bool ComputeBoundary(const Display& display_a,
                     const Display& display_b,
                     gfx::Rect* a_edge_in_screen,
                     gfx::Rect* b_edge_in_screen) {
  return ComputeBoundary(display_a.bounds(), display_b.bounds(),
                         a_edge_in_screen, b_edge_in_screen);
}

DisplayIdList CreateDisplayIdList(const Displays& list) {
  return GenerateDisplayIdList(list, &Display::id);
}

DisplayIdList CreateDisplayIdList(const DisplayInfoList& updated_displays) {
  return GenerateDisplayIdList(updated_displays,
                               &display::ManagedDisplayInfo::id);
}

void SortDisplayIdList(DisplayIdList* ids) {
  std::sort(ids->begin(), ids->end(),
            [](int64_t a, int64_t b) { return CompareDisplayIds(a, b); });
}

bool IsDisplayIdListSorted(const DisplayIdList& list) {
  return std::is_sorted(list.begin(), list.end(), [](int64_t a, int64_t b) {
    return CompareDisplayIds(a, b);
  });
}

std::string DisplayIdListToString(const DisplayIdList& list) {
  std::stringstream s;
  const char* sep = "";
  for (int64_t id : list) {
    s << sep << id;
    sep = ",";
  }
  return s.str();
}

int64_t GetDisplayIdWithoutOutputIndex(int64_t id) {
  constexpr uint64_t kMask = ~static_cast<uint64_t>(0xFF);
  return static_cast<int64_t>(kMask & id);
}

MixedMirrorModeParams::MixedMirrorModeParams(int64_t src_id,
                                             const DisplayIdList& dst_ids)
    : source_id(src_id), destination_ids(dst_ids) {}

MixedMirrorModeParams::MixedMirrorModeParams(
    const MixedMirrorModeParams& mixed_params) = default;

MixedMirrorModeParams::~MixedMirrorModeParams() = default;

MixedMirrorModeParamsErrors ValidateParamsForMixedMirrorMode(
    const DisplayIdList& connected_display_ids,
    const MixedMirrorModeParams& mixed_params) {
  if (connected_display_ids.size() <= 1) {
    return MixedMirrorModeParamsErrors::kErrorSingleDisplay;
  }

  std::set<int64_t> all_display_ids;
  for (auto& id : connected_display_ids) {
    all_display_ids.insert(id);
  }
  if (!all_display_ids.count(mixed_params.source_id)) {
    return MixedMirrorModeParamsErrors::kErrorSourceIdNotFound;
  }

  // This set is used to check duplicate id.
  std::set<int64_t> specified_display_ids;
  specified_display_ids.insert(mixed_params.source_id);

  if (mixed_params.destination_ids.empty()) {
    return MixedMirrorModeParamsErrors::kErrorDestinationIdsEmpty;
  }

  for (auto& id : mixed_params.destination_ids) {
    if (!all_display_ids.count(id)) {
      return MixedMirrorModeParamsErrors::kErrorDestinationIdNotFound;
    }
    if (!specified_display_ids.insert(id).second) {
      return MixedMirrorModeParamsErrors::kErrorDuplicateId;
    }
  }
  return MixedMirrorModeParamsErrors::kSuccess;
}

}  // namespace display
